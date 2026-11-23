#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern int main(void);

void Reset_Handler(void) {
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while (dst < &_edata) {
    *dst++ = *src++;
  }
  for (dst = &_sbss; dst < &_ebss;) {
    *dst++ = 0;
  }
  main();
  while (1)
    ;
}

void Default_Handler(void) {
  while (1)
    ;
}

/* Weak aliases: default to Default_Handler unless implemented elsewhere */
void NMI_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)  __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector"))) const uint32_t vector_table[] = {
    (uint32_t)&_estack,
    (uint32_t)Reset_Handler,
    (uint32_t)NMI_Handler,      /* NMI */
    (uint32_t)Default_Handler,  /* HardFault */
    (uint32_t)Default_Handler,  /* MemManage */
    (uint32_t)Default_Handler,  /* BusFault */
    (uint32_t)Default_Handler,  /* UsageFault */
    0,
    0,
    0,
    0,                          /* Reserved */
    (uint32_t)Default_Handler,  /* SVCall */
    (uint32_t)Default_Handler,  /* Debug Monitor */
    0,                          /* Reserved */
    (uint32_t)PendSV_Handler,   /* PendSV -> handled in qxk_port.c */
    (uint32_t)SysTick_Handler   /* SysTick -> handled in bsp.c */
};
