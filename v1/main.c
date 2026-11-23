#include "miros.h"
#include "bsp.h"

/* Stacks for threads */
static uint32_t stack_idle[64];
static uint32_t stack_blinky1[64];
static uint32_t stack_blinky2[64];
static uint32_t stack_blinky3[64];

static OSThread blinky1;
static OSThread blinky2;
static OSThread blinky3;

/* Thread 1: High Priority (5) - Sends Morse code "SOS" */
static void main_blinky1(void) {
    while (1) {
        BSP_sendMorseCode(0xA8EEE2A0U); /* "SOS" */
        OSThread_delay(20U);            /* 20 ticks pause so idle/TEST can run */
    }
}

/* Thread 2: Medium Priority (2) - Button burst task */
static void main_blinky2(void) {
    while (1) {
        uint32_t volatile i;

        /* wait for SW1 button semaphore */
        OSSema_wait(&SW1_sema);

        /* visible burst of toggling Blue LED */
        for (int count = 0; count < 6; ++count) {
            BSP_ledBlueOn();
            for (i = 50000U; i != 0U; --i) {}
            BSP_ledBlueOff();
            for (i = 50000U; i != 0U; --i) {}
        }
    }
}

/* Thread 3: Low Priority (1) - Sends Morse code "TEST" */
static void main_blinky3(void) {
    while (1) {
        BSP_sendMorseCode(0xE22A3800U); /* "TEST" */
        OSThread_delay(30U);            /* 30 ticks pause so idle/SOS can run */
    }
}

int main(void) {
    BSP_init();

    /* Initialize OS with idle stack */
    OS_init(stack_idle, sizeof(stack_idle));

    /* Initialize signaling semaphore for button SW1 */
    OSSema_init(&SW1_sema, 0U, 1U);

    /* Start the 3 threads with their respective priorities */
    OSThread_start(&blinky1, 5U, &main_blinky1, stack_blinky1, sizeof(stack_blinky1));
    OSThread_start(&blinky2, 2U, &main_blinky2, stack_blinky2, sizeof(stack_blinky2));
    OSThread_start(&blinky3, 1U, &main_blinky3, stack_blinky3, sizeof(stack_blinky3));

    /* Transfer control to the RTOS */
    OS_run();

    return 0;
}
