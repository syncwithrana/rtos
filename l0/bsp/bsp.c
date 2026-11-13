/* Board Support Package (BSP) for the ARM MPS2-AN385 board */
#include <stdint.h>

#include "bsp.h"
#include "mps2_an385.h"

/* on-board User LEDs on MPS2-AN385 (FPGAIO) */
#define LED_GREEN (1U << 0)
#define LED_RED (1U << 1)
#define LED_BLUE (1U << 2)

static uint32_t volatile l_tickCtr;

/* ISRs  ===============================================*/
void SysTick_Handler(void) { ++l_tickCtr; }

/* BSP functions ===========================================================*/
void BSP_init(void) {
  SystemCoreClockUpdate();
  SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);

  /* Turn off all LEDs */
  FPGAIO->LED = 0U;

  __enable_irq();
}

uint32_t BSP_tickCtr(void) {
  uint32_t tickCtr;

  __disable_irq();
  tickCtr = l_tickCtr;
  __enable_irq();

  return tickCtr;
}

void BSP_delay(uint32_t ticks) {
  uint32_t start = BSP_tickCtr();
  while ((BSP_tickCtr() - start) < ticks) {
  }
}

/*..........................................................................*/
void BSP_ledRedOn(void) { FPGAIO->LED |= LED_RED; }

void BSP_ledRedOff(void) { FPGAIO->LED &= ~LED_RED; }

void BSP_ledBlueOn(void) { FPGAIO->LED |= LED_BLUE; }

void BSP_ledBlueOff(void) { FPGAIO->LED &= ~LED_BLUE; }

void BSP_ledGreenOn(void) { FPGAIO->LED |= LED_GREEN; }

void BSP_ledGreenOff(void) { FPGAIO->LED &= ~LED_GREEN; }