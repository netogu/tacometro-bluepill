#include "stm32f1xx_ll_bus.h"
#include "stm32f1xx_ll_dma.h"
#include "stm32f1xx_ll_gpio.h"
#include "stm32f1xx_ll_rcc.h"
#include "stm32f1xx_ll_system.h"
#include "stm32f1xx_ll_tim.h"
#include "stm32f1xx_ll_usart.h"
#include "stm32f1xx_ll_utils.h"

#include "stdio.h"

/* PIN MAP */
#define LED_PORT GPIOC
#define LED_PIN LL_GPIO_PIN_13

#define TACO_PORT GPIOA
#define TACO_PIN LL_GPIO_PIN_0

#define UART_TX_PORT GPIOA
#define UART_TX_PIN LL_GPIO_PIN_9
#define UART_RX_PORT GPIOA
#define UART_RX_PIN LL_GPIO_PIN_10

void SystemClock_Config(void);
void LED_Init(void);
void USART1_Init(void);
void UART_SendString(const char *str);
void TIM2_Init(void);

// Esta variable es modificada por DMA, por eso es volatil
volatile uint16_t period_capture;

int main(void) {

  SystemClock_Config();
  LED_Init();
  USART1_Init();
  TIM2_Init();

  UART_SendString("\r\n --- Tacometro en Hardware (PA0) ---\r\n");

  while (1) {

    char tx_buff[64];

    // El programa lee el ultimo valor guardado en RAM for el DMA
    uint32_t period_us = (period_capture + 1) * 100;
    uint32_t hz = (1000000UL + (period_us / 2)) / period_us;
    uint32_t rpm = hz * 60UL;

    sprintf(tx_buff, "period: %u us | hz: %u | rpm: %u\r\n", period_us, hz,
            rpm);

    UART_SendString(tx_buff);

    LL_GPIO_TogglePin(LED_PORT, LED_PIN);

    LL_mDelay(200);
  }
}

void SystemClock_Config(void) {

  // Configuracion de clock:
  // 8MHz HSE -> 72MHz SysClk

  LL_RCC_HSE_Enable();
  while (LL_RCC_HSE_IsReady() != 1) {
  }

  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);

  // Input: HSE (8MHz) / 1 = 8MHz
  // Multiplier: 8MHz * 9 = 72MHz
  LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE_DIV_1, LL_RCC_PLL_MUL_9);

  LL_RCC_PLL_Enable();
  while (LL_RCC_PLL_IsReady() != 1) {
  }

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1); // 72Mhz
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);  // 36Mhz max!
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);  // 72Mhz
  //
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) {
  }

  SystemCoreClockUpdate();

  LL_Init1msTick(SystemCoreClock);
}

void LED_Init(void) {
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOC);

  LL_GPIO_SetPinMode(LED_PORT, LED_PIN, LL_GPIO_MODE_OUTPUT);
  LL_GPIO_SetPinSpeed(LED_PORT, LED_PIN, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinOutputType(LED_PORT, LED_PIN, LL_GPIO_OUTPUT_PUSHPULL);

  LL_GPIO_SetOutputPin(LED_PORT, LED_PIN);
}

void USART1_Init(void) {

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

  // TX (Alternate Function Push-Pull)
  LL_GPIO_SetPinMode(UART_TX_PORT, UART_TX_PIN, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinSpeed(UART_TX_PORT, UART_TX_PIN, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinOutputType(UART_TX_PORT, UART_TX_PIN, LL_GPIO_OUTPUT_PUSHPULL);

  // RX (Input Floating / Input Pull-up)
  LL_GPIO_SetPinMode(UART_RX_PORT, UART_RX_PIN, LL_GPIO_MODE_FLOATING);

  LL_USART_Disable(USART1);

  LL_USART_SetTransferDirection(USART1, LL_USART_DIRECTION_TX_RX);
  LL_USART_SetDataWidth(USART1, LL_USART_DATAWIDTH_8B);
  LL_USART_SetStopBitsLength(USART1, LL_USART_STOPBITS_1);
  LL_USART_SetParity(USART1, LL_USART_PARITY_NONE);

  LL_RCC_ClocksTypeDef rcc_clocks;
  LL_RCC_GetSystemClocksFreq(&rcc_clocks);
  LL_USART_SetBaudRate(USART1, rcc_clocks.PCLK2_Frequency,
                       115200); // Uses APB2 Clock

  LL_USART_Enable(USART1);
}

void UART_SendString(const char *str) {
  while (*str) {
    while (!LL_USART_IsActiveFlag_TXE(USART1)) {
    }

    LL_USART_TransmitData8(USART1, *str++);
  }
}

void TIM2_Init(void) {

  // Activar modulos
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_GPIOA);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  // Configuracion de puerto PA0 (TIM2_CH1)
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = TACO_PIN;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;
  LL_GPIO_Init(TACO_PORT, &GPIO_InitStruct);

  // Configuracion de timer
  LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
  LL_TIM_SetPrescaler(TIM2, 7200 - 1); // 100us
  LL_TIM_SetAutoReload(TIM2, 0xFFFF);

  // Configuracion de canal de entrada
  LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH1,
                           LL_TIM_ACTIVEINPUT_DIRECTTI);
  LL_TIM_IC_SetPrescaler(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_ICPSC_DIV1);
  LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_FILTER_FDIV1);
  LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_RISING);

  // Configuracion de modo
  LL_TIM_SetTriggerInput(TIM2, LL_TIM_TS_TI1FP1);
  LL_TIM_SetSlaveMode(TIM2, LL_TIM_SLAVEMODE_RESET);

  // Configuracion de DMA1 Channel 5 (TIM2_CH1)
  LL_DMA_ConfigAddresses(
      DMA1, LL_DMA_CHANNEL_5,
      (uint32_t)&(TIM2->CCR1),   // Origen: Timer Capture Register
      (uint32_t)&period_capture, // Destino: RAM
      LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_5, 1);

  // (Half-Word): CCRx son de 16bit
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_5, LL_DMA_PDATAALIGN_HALFWORD);
  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MDATAALIGN_HALFWORD);

  // Re-utilizar misma localidad de memoria
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MEMORY_NOINCREMENT);

  // Auto-reconfiguracion del DMA
  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_5, LL_DMA_MODE_CIRCULAR);

  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_5);

  LL_TIM_EnableDMAReq_CC1(TIM2);

  // Activar canal de entrada
  LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);

  // Activar timer
  LL_TIM_EnableCounter(TIM2);
}
