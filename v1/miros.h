#ifndef MIROS_H
#define MIROS_H

#include <stdint.h>

/* Thread Control Block (TCB) */
typedef struct {
    void *sp;             /* stack pointer */
    uint32_t timeout;     /* timeout delay in ticks */
    uint8_t prio;         /* thread priority (1..31, 0 is idle) */
} OSThread;

typedef void (*OSThreadHandler)(void);

/* OS Lifecycle */
void OS_init(void *stkSto, uint32_t stkSize);
void OS_sched(void);
void OS_run(void);
void OS_tick(void);
void OS_onStartup(void);
void OS_onIdle(void);

/* Thread Management */
void OSThread_start(
    OSThread *me,
    uint8_t prio,
    OSThreadHandler handler,
    void *stkSto,
    uint32_t stkSize);

void OSThread_delay(uint32_t ticks);

/* Signaling Semaphore */
typedef struct {
    uint8_t count;
    uint8_t max_count;
    uint32_t waitSet;
} OSSema;

void OSSema_init(OSSema *me, uint8_t count, uint8_t max_count);
void OSSema_wait(OSSema *me);
void OSSema_signal(OSSema *me);

/* Mutex with Priority-Ceiling Protocol */
typedef struct {
    uint8_t ceiling;
    uint8_t is_locked;
    OSThread *holder;
    uint8_t orig_prio;
    uint32_t waitSet;
} OSMutex;

void OSMutex_init(OSMutex *me, uint8_t ceiling);
void OSMutex_lock(OSMutex *me);
void OSMutex_unlock(OSMutex *me);

#endif /* MIROS_H */
