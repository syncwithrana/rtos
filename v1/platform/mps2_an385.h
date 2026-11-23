#ifndef MPS2_AN385_H
#define MPS2_AN385_H

#include <stdint.h>

/* System clock frequency: 25 MHz on QEMU mps2-an385 */
static uint32_t SystemCoreClock = 25000000U;

static inline void SystemCoreClockUpdate(void) {
    SystemCoreClock = 25000000U;
}

/* CMSIS Core Intrinsics */
static inline void __enable_irq(void) {
    __asm volatile ("cpsie i" : : : "memory");
}

static inline void __disable_irq(void) {
    __asm volatile ("cpsid i" : : : "memory");
}

/* Cortex-M3 Core SysTick & SCB Registers */
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010U)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014U)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018U)
#define SCB_AIRCR  (*(volatile uint32_t *)0xE000ED0CU)
#define SCB_SHPR3  (*(volatile uint32_t *)0xE000ED20U)

typedef enum {
    SysTick_IRQn = -1
} IRQn_Type;

static inline uint32_t SysTick_Config(uint32_t ticks) {
    SYST_RVR = ticks - 1U;
    SYST_CVR = 0U;
    SYST_CSR = 7U; /* Enable, tickint, core clock */
    return 0U;
}

static inline void NVIC_SetPriority(IRQn_Type IRQn, uint32_t priority) {
    if (IRQn == SysTick_IRQn) {
        SCB_SHPR3 = (SCB_SHPR3 & 0x00FFFFFFU) | (priority << 24U);
    }
}

static inline void NVIC_SystemReset(void) {
    __asm volatile ("dsb 0xF" : : : "memory");
    SCB_AIRCR = (0x5FAUL << 16U) | (1UL << 2U);
    __asm volatile ("dsb 0xF" : : : "memory");
    while (1) {
    }
}

/* FPGAIO System Control I/O Peripheral (Base: 0x40028000) */
typedef struct {
    volatile uint32_t LED;        /* 0x000: User LEDs (bits 0..N) */
    volatile uint32_t RESERVED0;  /* 0x004 */
    volatile uint32_t BUTTON;     /* 0x008: User buttons (bits 0..1) */
    volatile uint32_t RESERVED1;  /* 0x00C */
    volatile uint32_t CLK1HZ;     /* 0x010: 1Hz counter */
    volatile uint32_t CLK100HZ;   /* 0x014: 100Hz counter */
    volatile uint32_t COUNTER;    /* 0x018: Cycle counter */
    volatile uint32_t PRESCALE;   /* 0x01C */
    volatile uint32_t PSCNTR;     /* 0x020 */
} FPGAIO_Type;

#define FPGAIO_BASE 0x40028000U
#define FPGAIO      ((FPGAIO_Type *)FPGAIO_BASE)

#endif /* MPS2_AN385_H */
