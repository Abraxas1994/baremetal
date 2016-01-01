
#include "process.h"
#include "common.h"
#include "io.h"

static
ELLLIST thread_by_prio[4];
static
volatile process *process_current;

/* slow path for context switch.
 * if we arrive here then we know there will be a context switch.
 * the current thread's registers r4-r12 have already been stacked.
 * This call should return a value for LR in an exception handler (not nested)
 * to return to the next process.
 */
uint32_t thread_do_switch(uint32_t *frame)
{
    uint32_t ret = 0xfffffffd;
    thread *cur = thread_scheduler[0],
           *next= thread_scheduler[1];
    process *pnext= next->proc;

    cur->frame = (char*)frame;

    //TODO: check cur->active and do something?

    if(process_current!=pnext && !pnext->info->super) {
        // switching to a new non-privlaged process

        uint32_t mpurbar = M_SYS_BASE + 0xd9c; // MPU_RBAR0
        // load 8 32-bit words and write them to the 4 MPU region alias registers
        __asm__ ("ldmia %[thread], {r0-r7}\n"
                 "stmia %[mpu], {r0-r7}"
            :: [thread]"r"(pnext->mpu_settings),
               [mpu]"r"(mpurbar)
            : "r0",  "r1",  "r2",  "r3",
              "r4",  "r5",  "r6",  "r7"
        );

        process_current = pnext;
    } else {
        /* another thread in the same process, or a supervisor thread.
         * Leave the MPU alone.
         */
    }

    __asm__ volatile ("msr PSP, %0" :: "r"(next->frame):);

    thread_scheduler[0] = next;

    return ret;
}

void
thread_resume(thread *T)
{
    uint8_t prio = T->info->prio;
    thread *pending = (thread*)thread_scheduler[1];
    assert(T->holdcount && T->active);
    T->holdcount--;

    if(T->holdcount)
        return;

    if(prio>=NELEM(thread_by_prio))
        prio = NELEM(thread_by_prio);

    ellPushBack(&thread_by_prio[prio], &T->schednode);

    /* we have higher priority, take over */
    if(!pending || prio < pending->info->prio)
        thread_scheduler[1] = T;
}

static void thread_schedule_next(void)
{
    thread *next = NULL;
    unsigned i;

    for(i=0; i<NELEM(thread_by_prio); i++) {
        ELLNODE *node = ellFirst(&thread_by_prio[i]);
        if(!node)
            continue;
        next = container(node, thread, schednode);
        assert(next->active);
        assert(next->holdcount==0);
        break;
    }
    assert(next);
    thread_scheduler[1] = next;
}

void thread_suspend(thread *T)
{
    uint8_t prio = T->info->prio;
    if(T->holdcount==0) {
        ellRemove(&thread_by_prio[prio], &T->schednode);
    }
    T->holdcount++;

    thread_schedule_next();
}

void thread_yield(thread *T)
{
    uint8_t prio = T->info->prio;

    if(T->active) {
        assert(T->holdcount==0);
        ellRemove(&thread_by_prio[prio], &T->schednode);
        ellPushBack(&thread_by_prio[prio], &T->schednode);
    }
    thread_schedule_next();
}

#undef thread_scheduler
thread *thread_scheduler[2]; /* {current, next} */