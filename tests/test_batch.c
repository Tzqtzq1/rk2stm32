#include "batch_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
SPI_HandleTypeDef hspi1;
static int ack_level, cs_level = 1, ready_level, launches;
static uint32_t ticks;
static HAL_StatusTypeDef launch_result;
static uint8_t captured[BATCH_FRAME_SIZE];
void HAL_GPIO_WritePin(void *port, uint32_t pin, int level)
{ (void)port; (void)pin; ready_level = level; }
int HAL_GPIO_ReadPin(void *port, uint32_t pin)
{ (void)pin; return port == GPIOA ? cs_level : ack_level; }
void HAL_GPIO_Init(void *port, GPIO_InitTypeDef *gpio) { (void)port; (void)gpio; }
uint32_t HAL_GetTick(void) { return ticks; }
uint32_t HAL_SPI_GetError(SPI_HandleTypeDef *h) { (void)h; return 42; }
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *h, const uint8_t *p, uint16_t n)
{ (void)h; assert(n == sizeof(captured)); ++launches; memcpy(captured, p, n); return launch_result; }
static void reset(void)
{ ack_level = ticks = launches = 0; cs_level = 1; launch_result = HAL_OK; BatchLink_Init(); }
static void accept(void)
{
    BatchLink_OnTxDone(); BatchLink_Process();
    assert(ready_level); ack_level = 1; BatchLink_Process();
    assert(!ready_level); BatchLink_Process(); assert(!ready_level);
    ack_level = 0; BatchLink_Process();
}
int main(void)
{
    uint8_t data[3000]; batch_hdr_t hdr;
    memset(data, 0x5a, sizeof(data));
    assert(batch_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926u);
    reset(); assert(BatchLink_Write(data, 3000) == 2048);
    assert(BatchLink_Write(data + 2048, 952) == 0);
    BatchLink_Process(); assert(launches == 1 && ready_level);
    memcpy(&hdr, captured, sizeof(hdr));
    assert(hdr.seq == 1 && hdr.length == 1024);
    assert(hdr.crc32 == batch_frame_crc(&hdr, captured + sizeof(hdr)));
    hdr.seq++; assert(hdr.crc32 != batch_frame_crc(&hdr, captured + sizeof(hdr)));
    accept(); assert(launches == 2 && ready_level);
    assert(BatchLink_Write(data + 2048, 952) == 952);
    assert(BatchLink_Flush() == 0);
    accept(); memcpy(&hdr, captured, sizeof(hdr)); assert(hdr.length == 952 && hdr.seq == 3);
    accept(); assert(!ready_level);
    reset(); assert(BatchLink_Write(data, 7) == 7); BatchLink_Process(); assert(!launches);
    assert(BatchLink_Flush() == 0); launch_result = HAL_BUSY; BatchLink_Process();
    assert(BatchLink_Error() && !ready_level);
    launch_result = HAL_OK; assert(BatchLink_Recover() == 0); BatchLink_Process();
    memcpy(&hdr, captured, sizeof(hdr)); assert(hdr.length == 7 && hdr.seq == 1);
    cs_level = 0; BatchLink_OnTxDone(); ack_level = 1; BatchLink_Process(); assert(ready_level);
    cs_level = 1; BatchLink_Process(); assert(!ready_level);
    ack_level = 0; BatchLink_Process(); assert(!ready_level);
    reset(); BatchLink_Write(data, 1024); BatchLink_Process(); ticks = 6000; BatchLink_Process(); assert(!BatchLink_Error());
    cs_level = 0; BatchLink_Process(); ticks = 11000;
    BatchLink_Process(); assert(BatchLink_Error() && !ready_level);
    cs_level = 1; assert(BatchLink_Recover() == 0); BatchLink_Process(); assert(ready_level);
    BatchLink_OnError(); BatchLink_Process(); assert(BatchLink_Error() == 42);
    reset(); BatchLink_Write(data, 1024); BatchLink_Process();
    /* A CS pulse can end before Process sees it: EXTI must still start the timer. */
    BatchLink_OnNssFall(); ticks = 5000; BatchLink_Process();
    assert(BatchLink_Error() && !ready_level);
    reset(); BatchLink_Write(data, 1024); BatchLink_Process();
    BatchLink_OnTxDone(); BatchLink_Process(); ticks = 10000; BatchLink_Process();
    assert(BatchLink_Error() == 0x20000000u);
    assert(BatchLink_Recover() == 0); BatchLink_Process();
    memcpy(&hdr, captured, sizeof(hdr)); assert(hdr.seq == 1);
    assert(BatchLink_Write(NULL, 1) == -1);
    assert(BatchLink_Write(NULL, 0) == 0);
    puts("batch tests passed");
}
