/* pcie_atomic_shim.c -- software stand-in for device-issued PCIe atomics.
 *
 * The wall: ROCm's HSA signal model needs the GPU, as a PCIe requester, to issue
 * atomic read-modify-write to host memory. PCIe Gen1 (e.g. a Power Mac G5) cannot.
 *
 * The substrate: the GPU never issues an atomic. It posts a request (a plain,
 * ordered write the Gen1 link CAN route) into a per-queue SPSC mailbox. A single
 * host-side "completer" drains the mailboxes and performs the actual RMW. Because
 * exactly ONE agent ever touches the signal, atomicity comes from SERIALIZATION,
 * not from a hardware atomic op. This is the protocol that re-opens the HSA signal
 * layer on a fabric with no atomics.
 *
 * This program proves three things off-target:
 *   NATIVE : device has PCIe atomics (Gen3+)  -> correct, fast   (baseline)
 *   NAIVE  : Gen1, GPU just writes, no shim    -> CORRUPT (lost updates: the wall)
 *   SHIM   : Gen1 + this substrate             -> correct, slower (amortized)
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#define PRODUCERS          8
#define OPS_PER_PRODUCER   500000
#define RING               1024
#define TOTAL ((long)PRODUCERS * OPS_PER_PRODUCER)

static inline void relax(void){ __asm__ __volatile__("" ::: "memory"); }
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec + t.tv_nsec*1e-9; }

/* ---- per-producer SPSC mailbox (models a GPU queue's request ring) ---- */
typedef struct {
    int64_t operand;            /* plain stores by producer */
    _Atomic uint64_t valid;     /* producer RELEASE-publishes seq (the posted write) */
    _Atomic int64_t  ret;       /* completer writes the old value */
    _Atomic uint64_t done;      /* completer RELEASE-marks processed */
} slot_t;
static slot_t   mbox[PRODUCERS][RING];
static uint64_t ctail[PRODUCERS];           /* completer-owned tail per producer */

/* the HSA-style signal. SHIM/NAIVE use the plain one; NATIVE uses the atomic one. */
static int64_t          sig_plain;
static _Atomic int64_t  sig_atomic;

static int mode;                 /* 0 NATIVE, 1 NAIVE, 2 SHIM */
static _Atomic long processed;   /* completer progress (SHIM) */

static void* producer(void* arg){
    long id = (long)arg;
    uint64_t head = 0;
    for (long k=0; k<OPS_PER_PRODUCER; k++){
        if (mode==0){                       /* NATIVE: real PCIe atomic */
            atomic_fetch_add_explicit(&sig_atomic, -1, memory_order_relaxed);
        } else if (mode==1){                /* NAIVE: GPU plain RMW, no atomicity */
            int64_t v = sig_plain; relax(); sig_plain = v - 1;
        } else {                            /* SHIM: post to mailbox, completer does RMW */
            slot_t* s = &mbox[id][head % RING];
            while (atomic_load_explicit(&s->valid, memory_order_acquire) != 0) relax();
            s->operand = -1;                                   /* plain store */
            atomic_store_explicit(&s->done, 0, memory_order_relaxed);
            atomic_store_explicit(&s->valid, head+1, memory_order_release); /* publish */
            while (atomic_load_explicit(&s->done, memory_order_acquire)==0) relax();
            atomic_store_explicit(&s->valid, 0, memory_order_release);       /* free slot */
            head++;
        }
    }
    return NULL;
}

/* single host-side completer: the ONLY agent that touches the signal in SHIM mode */
static void* completer(void* arg){
    (void)arg;
    long done_total = 0;
    while (done_total < TOTAL){
        for (int p=0; p<PRODUCERS; p++){
            slot_t* s = &mbox[p][ctail[p] % RING];
            if (atomic_load_explicit(&s->valid, memory_order_acquire) == ctail[p]+1){
                int64_t old = sig_plain;          /* sole writer -> no atomic needed */
                sig_plain = old + s->operand;
                atomic_store_explicit(&s->ret, old, memory_order_relaxed);
                atomic_store_explicit(&s->done, 1, memory_order_release);
                ctail[p]++; done_total++;
            }
        }
    }
    atomic_store_explicit(&processed, done_total, memory_order_relaxed);
    return NULL;
}

static double run(int m, int64_t* final){
    mode=m; sig_plain=TOTAL; atomic_store(&sig_atomic, TOTAL);
    for (int p=0;p<PRODUCERS;p++){ ctail[p]=0; for(int i=0;i<RING;i++){ mbox[p][i].valid=0; } }
    pthread_t prod[PRODUCERS], comp;
    double t0=now();
    if (m==2) pthread_create(&comp,NULL,completer,NULL);
    for (long i=0;i<PRODUCERS;i++) pthread_create(&prod[i],NULL,producer,(void*)i);
    for (int i=0;i<PRODUCERS;i++) pthread_join(prod[i],NULL);
    if (m==2) pthread_join(comp,NULL);
    double dt=now()-t0;
    *final = (m==0)? atomic_load(&sig_atomic) : sig_plain;
    return dt;
}

int main(void){
    const char* name[]={"NATIVE (Gen3+ PCIe atomics)","NAIVE  (Gen1, no shim)","SHIM   (Gen1 + substrate)"};
    printf("%d producers x %d ops, signal starts at %ld, correct final = 0\n\n",
           PRODUCERS, OPS_PER_PRODUCER, TOTAL);
    printf("%-30s %14s %12s %14s\n","mode","final signal","verdict","Mops/sec");
    for (int m=0;m<3;m++){
        int64_t fin; double dt=run(m,&fin);
        printf("%-30s %14ld %12s %14.2f\n",
               name[m], (long)fin, fin==0?"CORRECT":"CORRUPT", (TOTAL/dt)/1e6);
    }
    return 0;
}
