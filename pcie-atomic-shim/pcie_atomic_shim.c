/* pcie_atomic_shim.c -- software stand-in for device-issued PCIe atomics.
 *
 * Wall: ROCm's HSA signal model needs the GPU, as a PCIe requester, to issue
 * atomic read-modify-write to host memory. PCIe Gen1 (Power Mac G5) cannot.
 *
 * Substrate: the GPU never issues an atomic. It posts a request (a plain, ordered
 * write the Gen1 link CAN route) into a per-queue SPSC mailbox. A single host-side
 * completer drains the mailboxes and performs the RMW. One agent ever touches the
 * signal, so atomicity comes from SERIALIZATION, not a hardware atomic op.
 *
 * SCOPE OF THIS PROOF (tri-brain hardened, Codex):
 *   It proves the SERIALIZATION PROTOCOL is correct and cheap, off-target on x86-64.
 *   It does NOT prove: (a) the big-endian wire format on the real target (both
 *   sides here are little-endian x86), nor (b) full HSA acquire/release/wait
 *   semantics. Those are identified, bounded, and NOT demonstrated here. The
 *   byte-order contract is encoded below (le64_wire/le64_host) so the BE site is
 *   explicit even though x86 exercises it as a no-op.
 *
 *   NATIVE : device has PCIe atomics (Gen3+)        -> correct, fast   (baseline)
 *   NAIVE  : Gen1, separate non-atomic load+store    -> CORRUPT (lost updates: the wall)
 *   SHIM   : Gen1 + this substrate                   -> correct, slower (amortized)
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

/* Mailbox fields are LITTLE-ENDIAN WIRE FORMAT: the GPU (LE) posts; a BE host
 * byteswaps on read. On x86 (LE) these are no-ops, so this proof does NOT exercise
 * the BE path -- it only declares the contract at the right site. */
static inline int64_t le64_wire(int64_t v){           /* producer (GPU) -> wire */
#if defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__==__ORDER_BIG_ENDIAN__)
    return (int64_t)__builtin_bswap64((uint64_t)v);
#else
    return v;
#endif
}
static inline int64_t le64_host(int64_t v){ return le64_wire(v); } /* wire -> host (BE) */

/* ---- per-producer SPSC mailbox (models a GPU queue's request ring) ---- */
typedef struct {
    int64_t operand;            /* LE wire; plain stores by producer */
    _Atomic uint64_t valid;     /* producer RELEASE-publishes seq (the posted write) */
    _Atomic int64_t  ret;       /* completer writes old value */
    _Atomic uint64_t done;      /* completer RELEASE-marks processed */
} slot_t;
static slot_t   mbox[PRODUCERS][RING];
static uint64_t ctail[PRODUCERS];

static int64_t          sig_plain;        /* SHIM: only the completer touches it */
static _Atomic int64_t  sig_atomic;       /* NATIVE: real atomic RMW */
static _Atomic int64_t  sig_naive;        /* NAIVE: relaxed load+store, no RMW atomicity (DEFINED) */
static int mode;

static void* producer(void* arg){
    long id = (long)arg; uint64_t head = 0;
    for (long k=0; k<OPS_PER_PRODUCER; k++){
        if (mode==0){
            atomic_fetch_add_explicit(&sig_atomic, -1, memory_order_relaxed);
        } else if (mode==1){
            /* DEFINED model of "no atomic RMW": relaxed load then relaxed store.
             * No data race (atomic accesses), but concurrent producers lose updates. */
            int64_t v = atomic_load_explicit(&sig_naive, memory_order_relaxed);
            relax();
            atomic_store_explicit(&sig_naive, v-1, memory_order_relaxed);
        } else {
            slot_t* s = &mbox[id][head % RING];
            while (atomic_load_explicit(&s->valid, memory_order_acquire) != 0) relax();
            s->operand = le64_wire(-1);                        /* LE wire store */
            atomic_store_explicit(&s->done, 0, memory_order_relaxed);
            atomic_store_explicit(&s->valid, head+1, memory_order_release);
            while (atomic_load_explicit(&s->done, memory_order_acquire)==0) relax();
            atomic_store_explicit(&s->valid, 0, memory_order_release);
            head++;
        }
    }
    return NULL;
}

static void* completer(void* arg){
    (void)arg; long done_total = 0;
    while (done_total < TOTAL){
        for (int p=0; p<PRODUCERS; p++){
            slot_t* s = &mbox[p][ctail[p] % RING];
            if (atomic_load_explicit(&s->valid, memory_order_acquire) == ctail[p]+1){
                int64_t op = le64_host(s->operand);   /* BE byteswap site (no-op on x86) */
                int64_t old = sig_plain;              /* sole writer -> no atomic needed */
                sig_plain = old + op;
                atomic_store_explicit(&s->ret, old, memory_order_relaxed);
                atomic_store_explicit(&s->done, 1, memory_order_release);
                ctail[p]++; done_total++;
            }
        }
    }
    return NULL;
}

static double run(int m, int64_t* final){
    mode=m; sig_plain=TOTAL; atomic_store(&sig_atomic,TOTAL); atomic_store(&sig_naive,TOTAL);
    for (int p=0;p<PRODUCERS;p++){ ctail[p]=0; for(int i=0;i<RING;i++) mbox[p][i].valid=0; }
    pthread_t prod[PRODUCERS], comp; double t0=now();
    if (m==2 && pthread_create(&comp,NULL,completer,NULL)!=0){ perror("completer"); exit(2); }
    for (long i=0;i<PRODUCERS;i++)
        if (pthread_create(&prod[i],NULL,producer,(void*)i)!=0){ perror("producer"); exit(2); }
    for (int i=0;i<PRODUCERS;i++) pthread_join(prod[i],NULL);
    if (m==2) pthread_join(comp,NULL);
    double dt=now()-t0;
    *final = (m==0)? atomic_load(&sig_atomic) : (m==1)? atomic_load(&sig_naive) : sig_plain;
    return dt;
}

int main(void){
    const char* name[]={"NATIVE (Gen3+ PCIe atomics)","NAIVE  (Gen1, no atomic RMW)","SHIM   (Gen1 + substrate)"};
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
