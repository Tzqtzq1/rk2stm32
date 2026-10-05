#include "batch_link.h"
#include <string.h>

/* SPI1 slave, 8 bits, mode 0, hardware NSS PA4, TX DMA1 channel 3.
 * PB0 READY output; PB1 ACK input with pulldown. One main-loop producer.
 * HAL_SPI_TxCpltCallback -> OnTxDone; HAL_SPI_ErrorCallback -> OnError.
 * Process does CRC, DMA startup and handshake outside interrupt callbacks.
 */
extern SPI_HandleTypeDef hspi1;
static uint8_t slots[2][BATCH_MAX_PAYLOAD];
static uint8_t frame[BATCH_FRAME_SIZE];
static uint16_t lengths[2];
static uint8_t head, count;
static uint32_t sequence;
static volatile uint8_t done, fault;
static volatile uint32_t error;
enum { IDLE, ACTIVE, WAIT_ACK, WAIT_ACK_LOW, FAILED };
static volatile uint8_t state;
static volatile uint32_t active_since;
static volatile uint8_t clock_started;

static void ready(int high)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
static int ack(void) { return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_SET; }
static int selected(void) { return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET; }

void BatchLink_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    ready(0);
    gpio.Pin = GPIO_PIN_0;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &gpio);
    memset(lengths, 0, sizeof(lengths));
    head = count = done = fault = 0;
    error = 0;
    sequence = 1;
    state = IDLE;
}

int BatchLink_Write(const uint8_t *data, uint16_t len)
{
    uint16_t accepted = 0;
    if (!data && len) return -1;
    while (accepted < len && count < 2) {
        uint8_t tail = (uint8_t)((head + count) % 2);
        uint16_t n = (uint16_t)(BATCH_MAX_PAYLOAD - lengths[tail]);
        if (n > len - accepted) n = (uint16_t)(len - accepted);
        memcpy(slots[tail] + lengths[tail], data + accepted, n);
        lengths[tail] = (uint16_t)(lengths[tail] + n);
        accepted = (uint16_t)(accepted + n);
        if (lengths[tail] == BATCH_MAX_PAYLOAD) ++count;
    }
    return accepted;
}

int BatchLink_Flush(void)
{
    uint8_t tail;
    if (count == 2) return -1;
    tail = (uint8_t)((head + count) % 2);
    if (lengths[tail]) ++count;
    return 0;
}

void BatchLink_Process(void)
{
    batch_hdr_t hdr;
    HAL_StatusTypeDef result;
    if (fault) {
        ready(0);
        state = FAILED;
        return;
    }
    if (state == ACTIVE) {
        if (selected() && !clock_started) {
            clock_started = 1;
            active_since = HAL_GetTick();
        }
        if (done && !selected()) {
            state = WAIT_ACK;
            active_since = HAL_GetTick();
        }
        else if (clock_started && (uint32_t)(HAL_GetTick() - active_since) >= 5000u) {
            error = 0x80000000u;
            fault = 1;
            ready(0);
            state = FAILED;
        }
    }
    if (state == WAIT_ACK && ack()) {
        ready(0);
        state = WAIT_ACK_LOW;
        active_since = HAL_GetTick();
    }
    if (state == WAIT_ACK_LOW && !ack()) {
        lengths[head] = 0;
        head ^= 1u;
        --count;
        ++sequence;
        state = IDLE;
    }
    if ((state == WAIT_ACK || state == WAIT_ACK_LOW) &&
        (uint32_t)(HAL_GetTick() - active_since) >= 10000u) {
        error = 0x20000000u;
        fault = 1;
        ready(0);
        state = FAILED;
    }
    if (state != IDLE || !count || ack() || selected()) return;
    hdr.magic = BATCH_MAGIC;
    hdr.seq = sequence;
    hdr.length = lengths[head];
    hdr.crc32 = batch_frame_crc(&hdr, slots[head]);
    memcpy(frame, &hdr, sizeof(hdr));
    memcpy(frame + sizeof(hdr), slots[head], lengths[head]);
    memset(frame + sizeof(hdr) + lengths[head], 0, BATCH_MAX_PAYLOAD - lengths[head]);
    done = 0;
    state = ACTIVE;
    active_since = HAL_GetTick();
    clock_started = 0;
    result = HAL_SPI_Transmit_DMA(&hspi1, frame, (uint16_t)sizeof(frame));
    if (result != HAL_OK) {
        error = 0x40000000u | (uint32_t)result;
        fault = 1;
        state = FAILED;
        return;
    }
    if (!fault) ready(1);
}
void BatchLink_OnNssFall(void)
{
    if (state == ACTIVE && !clock_started) {
        active_since = HAL_GetTick();
        clock_started = 1;
    }
}
void BatchLink_OnTxDone(void) { done = 1; }
void BatchLink_OnError(void)
{
    error = HAL_SPI_GetError(&hspi1);
    if (!error) error = 1;
    fault = 1;
    ready(0);
}
uint32_t BatchLink_Error(void) { return error; }
int BatchLink_Recover(void)
{
    if (state != FAILED || ack() || selected()) return -1;
    if (HAL_SPI_Abort(&hspi1) != HAL_OK) return -1;
    done = fault = 0;
    error = 0;
    state = IDLE;
    return 0;
}
