#ifndef BATCH_LINK_H
#define BATCH_LINK_H
#include "stm32f1xx_hal.h"
#include "../common/batch_protocol.h"
void BatchLink_Init(void);
/* Single producer, main-loop only. Returns accepted byte count, or -1 for invalid input.
 * Retry only data + accepted, len - accepted. Zero means backpressure. */
int BatchLink_Write(const uint8_t *data, uint16_t len);
/* Queue a partial final batch. Returns 0, or -1 when both slots are occupied. */
int BatchLink_Flush(void);
/* Call frequently from the main loop, never from an ISR. */
void BatchLink_Process(void);
/* Call from SPI1 HAL callbacks. */
void BatchLink_OnTxDone(void);
void BatchLink_OnError(void);
/* PA4 NSS falling-edge EXTI callback: detects even a short interrupted transfer. */
void BatchLink_OnNssFall(void);
/* Nonzero HAL error/start failure. Retains queued data. */
uint32_t BatchLink_Error(void);
/* Call after fixing the peripheral fault; CS and ACK must be low/inactive. */
int BatchLink_Recover(void);
#endif
