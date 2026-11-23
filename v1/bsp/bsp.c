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

/* Internal LED state mirror (all 4 bits: G:0, R:1, B:2, Y:3) */
static uint32_t s_led_state = 0U;

static void BSP_setLedState(uint32_t set_mask, uint32_t clr_mask) {
    s_led_state = (s_led_state | set_mask) & ~clr_mask;
    FPGAIO->LED = s_led_state & 0x03U;       /* physical 2-bit FPGAIO LEDs */
    FPGAIO->PRESCALE = s_led_state;          /* full 32-bit register monitored by GDB */
}

/* SysTick ISR */
void SysTick_Handler(void) {
    static uint32_t btn_sim_ticks = 0U;

    BSP_ledYellowOff();

    OS_tick();

    /* Simulate periodic SW1 button press every 30 ticks in QEMU */
    if (++btn_sim_ticks >= 30U) {
        btn_sim_ticks = 0U;
        OSSema_signal(&SW1_sema);
    }
}

void BSP_init(void) {
    s_led_state = 0U;
    FPGAIO->LED = 0U;
    FPGAIO->PRESCALE = 0U;

    /* Initialize Morse mutex with ceiling priority 6 */
    OSMutex_init(&Morse_mutex, 6U);
}

void BSP_ledGreenOn(void)    { BSP_setLedState(LED_GREEN, 0U); }
void BSP_ledGreenOff(void)   { BSP_setLedState(0U, LED_GREEN); }
void BSP_ledGreenToggle(void){ s_led_state & LED_GREEN ? BSP_ledGreenOff() : BSP_ledGreenOn(); }

void BSP_ledRedOn(void)      { BSP_setLedState(LED_RED, 0U); }
void BSP_ledRedOff(void)     { BSP_setLedState(0U, LED_RED); }
void BSP_ledRedToggle(void)  { s_led_state & LED_RED ? BSP_ledRedOff() : BSP_ledRedOn(); }

void BSP_ledBlueOn(void)     { BSP_setLedState(LED_BLUE, 0U); }
void BSP_ledBlueOff(void)    { BSP_setLedState(0U, LED_BLUE); }
void BSP_ledBlueToggle(void) { s_led_state & LED_BLUE ? BSP_ledBlueOff() : BSP_ledBlueOn(); }

void BSP_ledYellowOn(void)   { BSP_setLedState(LED_YELLOW, 0U); }
void BSP_ledYellowOff(void)  { BSP_setLedState(0U, LED_YELLOW); }
void BSP_ledYellowToggle(void){ s_led_state & LED_YELLOW ? BSP_ledYellowOff() : BSP_ledYellowOn(); }

void BSP_sendMorseCode(uint32_t bitmask) {
    uint32_t volatile delay_ctr;
    /* Balanced timing for smooth LED display in QEMU */
    enum { DOT_DELAY = 25000 };

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
