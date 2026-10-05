#include "batch_link.h"
SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;
/* Visible in debugger. Fail closed; queued data remains available. */
volatile uint32_t app_error;
void _init(void) {}
void _fini(void) {}
static void fatal(void) { app_error = 0xffffffffu; __disable_irq(); for (;;) {} }
void HAL_SPI_MspInit(SPI_HandleTypeDef *spi)
{
    GPIO_InitTypeDef gpio = {0};
    if (spi->Instance != SPI1) return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_4; gpio.Pull = GPIO_PULLUP;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    HAL_GPIO_Init(GPIOA, &gpio);
    gpio.Pin = GPIO_PIN_6;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
    hdma_spi1_tx.Instance = DMA1_Channel3;
    hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_spi1_tx.Init.Mode = DMA_NORMAL;
    hdma_spi1_tx.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_spi1_tx) != HAL_OK) fatal();
    __HAL_LINKDMA(spi, hdmatx, hdma_spi1_tx);
    /* SysTick must preempt HAL timeout loops inside DMA callbacks. */
    HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
    HAL_NVIC_SetPriority(SPI1_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
}
void SysTick_Handler(void) { HAL_IncTick(); }
void DMA1_Channel3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_spi1_tx); }
void EXTI4_IRQHandler(void) { HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_4); }
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{ if (pin == GPIO_PIN_4) BatchLink_OnNssFall(); }
void SPI1_IRQHandler(void) { HAL_SPI_IRQHandler(&hspi1); }
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *spi)
{ if (spi->Instance == SPI1) BatchLink_OnTxDone(); }
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi)
{ if (spi->Instance == SPI1) BatchLink_OnError(); }
#if BATCH_DEMO
static uint8_t demo[2500];
static uint16_t demo_offset;
static uint8_t demo_flushed;
#endif
int main(void)
{
    /* Reset clock: internal HSI 8 MHz, no external crystal required. */
    HAL_Init();
    HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_SLAVE;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    if (HAL_SPI_Init(&hspi1) != HAL_OK) fatal();
    BatchLink_Init();
#if BATCH_DEMO
    for (uint16_t i = 0; i < sizeof(demo); ++i) demo[i] = (uint8_t)i;
#endif
    for (;;) {
        /* Insert acquisition here; preserve any bytes Write did not accept.
         * Flush at acquisition end. Never feed synthetic data by default. */
        BatchLink_Process();
        app_error = BatchLink_Error();
#if BATCH_DEMO
        if (!app_error && demo_offset < sizeof(demo)) {
            int accepted = BatchLink_Write(demo + demo_offset,
                (uint16_t)(sizeof(demo) - demo_offset));
            if (accepted >= 0) demo_offset += (uint16_t)accepted;
        }
        if (!app_error && demo_offset == sizeof(demo) && !demo_flushed &&
            BatchLink_Flush() == 0) demo_flushed = 1;
#endif
    }
}
