#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include "miros.h"

/* System clock tick rate [Hz] */
#define BSP_TICKS_PER_SEC 100U

void BSP_init(void);

void BSP_ledRedOn(void);
void BSP_ledRedOff(void);
void BSP_ledRedToggle(void);

void BSP_ledBlueOn(void);
void BSP_ledBlueOff(void);
void BSP_ledBlueToggle(void);

void BSP_ledGreenOn(void);
void BSP_ledGreenOff(void);
void BSP_ledGreenToggle(void);

void BSP_ledYellowOn(void);
void BSP_ledYellowOff(void);
void BSP_ledYellowToggle(void);

void BSP_sendMorseCode(uint32_t bitmask);

extern OSSema SW1_sema;

#endif /* BSP_H */
