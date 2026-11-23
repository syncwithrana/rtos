#include <stdint.h>
#include "miros.h"

/* Global OS variables */
OSThread *OS_currThread;
OSThread *OS_nextThread;

static OSThread *OS_threads[32];
static uint32_t OS_readySet;
static uint32_t OS_delayedSet;

static OSThread OS_idleThread;

static void OS_idleThreadHandler(void) {
    for (;;) {
        OS_onIdle();
    }
}

void OS_init(void *stkSto, uint32_t stkSize) {
    OS_readySet   = 0U;
    OS_delayedSet = 0U;

    for (uint32_t i = 0U; i < 32U; ++i) {
        OS_threads[i] = (OSThread *)0;
    }

    /* Start the idle thread at priority 0 */
    OSThread_start(&OS_idleThread, 0U, &OS_idleThreadHandler, stkSto, stkSize);
}

void OSThread_start(
    OSThread *me,
    uint8_t prio,
    OSThreadHandler handler,
    void *stkSto,
    uint32_t stkSize)
{
    /* Round down stack top to 8-byte boundary */
    uint32_t *sp = (uint32_t *)((((uint32_t)stkSto + stkSize) >> 3U) << 3U);

    /* Fabricate ARM Cortex-M exception stack frame */
    *(--sp) = (1U << 24);                 /* xPSR (Thumb bit) */
    *(--sp) = ((uint32_t)handler & ~1U);  /* PC (halfword aligned) */
    *(--sp) = 0x0000000EU;                /* LR */
    *(--sp) = 0x0000000CU;                /* R12 */
    *(--sp) = 0x00000003U;                /* R3 */
    *(--sp) = 0x00000002U;                /* R2 */
    *(--sp) = 0x00000001U;                /* R1 */
    *(--sp) = 0x00000000U;                /* R0 */
    /* Software-saved registers (R4-R11) */
    *(--sp) = 0x0000000BU;                /* R11 */
    *(--sp) = 0x0000000AU;                /* R10 */
    *(--sp) = 0x00000009U;                /* R9 */
    *(--sp) = 0x00000008U;                /* R8 */
    *(--sp) = 0x00000007U;                /* R7 */
    *(--sp) = 0x00000006U;                /* R6 */
    *(--sp) = 0x00000005U;                /* R5 */
    *(--sp) = 0x00000004U;                /* R4 */

    me->sp      = sp;
    me->prio    = prio;
    me->timeout = 0U;

    OS_threads[prio] = me;
    OS_readySet |= (1U << prio);
}

void OS_sched(void) {
    if (OS_readySet == 0U) {
        OS_nextThread = OS_threads[0];
    } else {
        uint8_t p = 31U - (uint8_t)__builtin_clz(OS_readySet);
        OS_nextThread = OS_threads[p];
    }

    if (OS_nextThread != OS_currThread) {
        /* Pend PendSV: bit 28 of ICSR at 0xE000ED04 */
        *(volatile uint32_t *)0xE000ED04U = (1U << 28U);
    }
}

void OS_run(void) {
    OS_onStartup();

    __asm volatile ("cpsid i" : : : "memory");
    OS_sched();
    __asm volatile ("cpsie i" : : : "memory");

    for (;;) {
        OS_onIdle();
    }
}

void OS_tick(void) {
    uint32_t workingSet = OS_delayedSet;
    while (workingSet != 0U) {
        uint8_t p = 31U - (uint8_t)__builtin_clz(workingSet);
        workingSet &= ~(1U << p);
        if (OS_threads[p]->timeout > 0U) {
            --OS_threads[p]->timeout;
            if (OS_threads[p]->timeout == 0U) {
                OS_delayedSet &= ~(1U << p);
                OS_readySet   |= (1U << p);
            }
        }
    }
    OS_sched();
}

void OSThread_delay(uint32_t ticks) {
    __asm volatile ("cpsid i" : : : "memory");
    if (ticks > 0U) {
        OS_currThread->timeout = ticks;
        OS_readySet   &= ~(1U << OS_currThread->prio);
        OS_delayedSet |= (1U << OS_currThread->prio);
        OS_sched();
    }
    __asm volatile ("cpsie i" : : : "memory");
}

/* Semaphore Implementation */
void OSSema_init(OSSema *me, uint8_t count, uint8_t max_count) {
    me->count     = count;
    me->max_count = max_count;
    me->waitSet   = 0U;
}

void OSSema_wait(OSSema *me) {
    __asm volatile ("cpsid i" : : : "memory");
    if (me->count > 0U) {
        --me->count;
        __asm volatile ("cpsie i" : : : "memory");
    } else {
        OS_readySet &= ~(1U << OS_currThread->prio);
        me->waitSet |= (1U << OS_currThread->prio);
        OS_sched();
        __asm volatile ("cpsie i" : : : "memory");
    }
}

void OSSema_signal(OSSema *me) {
    __asm volatile ("cpsid i" : : : "memory");
    if (me->waitSet != 0U) {
        uint8_t p = 31U - (uint8_t)__builtin_clz(me->waitSet);
        me->waitSet &= ~(1U << p);
        OS_readySet |= (1U << p);
        OS_sched();
    } else {
        if (me->count < me->max_count) {
            ++me->count;
        }
    }
    __asm volatile ("cpsie i" : : : "memory");
}

/* Mutex with Priority-Ceiling Protocol (PCP) */
void OSMutex_init(OSMutex *me, uint8_t ceiling) {
    me->ceiling   = ceiling;
    me->is_locked = 0U;
    me->holder    = (OSThread *)0;
    me->orig_prio = 0U;
    me->waitSet   = 0U;
}

void OSMutex_lock(OSMutex *me) {
    __asm volatile ("cpsid i" : : : "memory");
    if (!me->is_locked) {
        me->is_locked  = 1U;
        me->holder     = OS_currThread;
        me->orig_prio  = OS_currThread->prio;

        /* Boost holder priority to ceiling */
        if (me->ceiling > OS_currThread->prio) {
            OS_readySet &= ~(1U << OS_currThread->prio);
            OS_threads[OS_currThread->prio] = (OSThread *)0;

            OS_currThread->prio = me->ceiling;
            OS_threads[me->ceiling] = OS_currThread;
            OS_readySet |= (1U << me->ceiling);
        }
        __asm volatile ("cpsie i" : : : "memory");
    } else {
        /* Block until released */
        OS_readySet &= ~(1U << OS_currThread->prio);
        me->waitSet |= (1U << OS_currThread->prio);
        OS_sched();
        __asm volatile ("cpsie i" : : : "memory");
    }
}

void OSMutex_unlock(OSMutex *me) {
    __asm volatile ("cpsid i" : : : "memory");
    if (me->holder == OS_currThread) {
        /* Restore original priority */
        if (OS_currThread->prio != me->orig_prio) {
            OS_readySet &= ~(1U << OS_currThread->prio);
            OS_threads[OS_currThread->prio] = (OSThread *)0;

            OS_currThread->prio = me->orig_prio;
            OS_threads[me->orig_prio] = OS_currThread;
            OS_readySet |= (1U << me->orig_prio);
        }

        me->is_locked = 0U;
        me->holder    = (OSThread *)0;

        /* Unblock highest-priority waiting thread */
        if (me->waitSet != 0U) {
            uint8_t p = 31U - (uint8_t)__builtin_clz(me->waitSet);
            me->waitSet &= ~(1U << p);
            OS_readySet |= (1U << p);
        }

        OS_sched();
    }
    __asm volatile ("cpsie i" : : : "memory");
}

/* Cortex-M3 PendSV Handler */
__attribute__((naked))
void PendSV_Handler(void) {
    __asm volatile (
        "  CPSID   i                    \n"
        "  MRS     r0, PSP              \n"
        "  CBZ     r0, PendSV_restore   \n"
        "  STMDB   r0!, {r4-r11}        \n"
        "  LDR     r1, =OS_currThread   \n"
        "  LDR     r1, [r1]             \n"
        "  STR     r0, [r1]             \n"
        "PendSV_restore:                \n"
        "  LDR     r1, =OS_nextThread   \n"
        "  LDR     r1, [r1]             \n"
        "  LDR     r2, =OS_currThread   \n"
        "  STR     r1, [r2]             \n"
        "  LDR     r0, [r1]             \n"
        "  LDMIA   r0!, {r4-r11}        \n"
        "  MSR     PSP, r0              \n"
        "  CPSIE   i                    \n"
        "  MVN     lr, #2               \n"
        "  BX      lr                   \n"
    );
}
