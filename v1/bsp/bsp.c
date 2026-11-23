/* Board Support Package (BSP) for ARM MPS2-AN385 board on QEMU */
#include <stdint.h>
#include "bsp.h"
#include "mps2_an385.h"
#include "miros.h"

/* MPS2-AN385 FPGAIO User LEDs */
#define LED_GREEN  (1U << 0)
#define LED_RED    (1U << 1)
#define LED_BLUE   (1U << 2)
#define LED_YELLOW (1U << 3)

OSSema SW1_sema;

/* Mutex with Priority-Ceiling Protocol protecting Morse transmission */
static OSMutex Morse_mutex;

/* SysTick ISR */
void SysTick_Handler(void) {
    static uint32_t btn_sim_ticks = 0U;

    OS_tick();

    /* Simulate periodic SW1 button press every 50 ticks in QEMU */
    if (++btn_sim_ticks >= 50U) {
        btn_sim_ticks = 0U;
        OSSema_signal(&SW1_sema);
    }
}

void BSP_init(void) {
    /* Clear all LEDs */
    FPGAIO->LED = 0U;

    /* Initialize Morse mutex with ceiling priority 6 */
    OSMutex_init(&Morse_mutex, 6U);
}

void BSP_ledRedOn(void)      { FPGAIO->LED |= LED_RED; }
void BSP_ledRedOff(void)     { FPGAIO->LED &= ~LED_RED; }
void BSP_ledRedToggle(void)  { FPGAIO->LED ^= LED_RED; }

void BSP_ledBlueOn(void)     { FPGAIO->LED |= LED_BLUE; }
void BSP_ledBlueOff(void)    { FPGAIO->LED &= ~LED_BLUE; }
void BSP_ledBlueToggle(void) { FPGAIO->LED ^= LED_BLUE; }

void BSP_ledGreenOn(void)    { FPGAIO->LED |= LED_GREEN; }
void BSP_ledGreenOff(void)   { FPGAIO->LED &= ~LED_GREEN; }
void BSP_ledGreenToggle(void){ FPGAIO->LED ^= LED_GREEN; }

void BSP_ledYellowOn(void)   { FPGAIO->LED |= LED_YELLOW; }
void BSP_ledYellowOff(void)  { FPGAIO->LED &= ~LED_YELLOW; }
void BSP_ledYellowToggle(void){ FPGAIO->LED ^= LED_YELLOW; }

void BSP_sendMorseCode(uint32_t bitmask) {
    uint32_t volatile delay_ctr;
    /* Balanced timing for smooth LED display in QEMU */
    enum { DOT_DELAY = 35000 };

    /* Lock mutex (Priority-Ceiling Protocol raises priority to 6) */
    OSMutex_lock(&Morse_mutex);

    for (; bitmask != 0U; bitmask <<= 1) {
        if ((bitmask & (1U << 31)) != 0U) {
            BSP_ledGreenOn();
        } else {
            BSP_ledGreenOff();
        }
        for (delay_ctr = DOT_DELAY; delay_ctr != 0U; --delay_ctr) {
        }
    }
    BSP_ledGreenOff();
    for (delay_ctr = 2U * DOT_DELAY; delay_ctr != 0U; --delay_ctr) {
    }

    OSMutex_unlock(&Morse_mutex);
}

/* OS Callbacks */
void OS_onStartup(void) {
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);

    /* Set both PendSV and SysTick to lowest priority (0xFF)
     * PendSV (PRI_14: bits 23:16), SysTick (PRI_15: bits 31:24).
     * This prevents PendSV from preempting SysTick while active on MSP.
     */
    SCB_SHPR3 |= (0xFFU << 16U) | (0xFFU << 24U);
}

void OS_onIdle(void) {
    /* Turn on Yellow LED while waiting in idle, turn off when active */
    BSP_ledYellowOn();
    __asm volatile ("wfi");
    BSP_ledYellowOff();
}
