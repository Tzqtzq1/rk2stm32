#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
typedef struct { int unused; } SPI_HandleTypeDef;
typedef struct { uint32_t Pin, Mode, Speed, Pull; } GPIO_InitTypeDef;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY } HAL_StatusTypeDef;
#define GPIOA ((void*)1)
#define GPIOB ((void*)2)
#define GPIO_PIN_0 1
#define GPIO_PIN_1 2
#define GPIO_PIN_4 16
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_MODE_INPUT 0
#define GPIO_PULLDOWN 2
#define GPIO_SPEED_FREQ_LOW 0
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
void HAL_GPIO_WritePin(void *, uint32_t, int);
int HAL_GPIO_ReadPin(void *, uint32_t);
void HAL_GPIO_Init(void *, GPIO_InitTypeDef *);
uint32_t HAL_GetTick(void);
uint32_t HAL_SPI_GetError(SPI_HandleTypeDef *);
HAL_StatusTypeDef HAL_SPI_Abort(SPI_HandleTypeDef *);
HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *, const uint8_t *, uint16_t);
#endif
