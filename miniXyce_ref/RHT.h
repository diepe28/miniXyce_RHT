//
// Created by diego on 30/10/17.
//

// -D CMAKE_C_COMPILER=/usr/bin/clang-5.0 -D CMAKE_CXX_COMPILER=/usr/bin/clang++-5.0
// -D CMAKE_C_COMPILER=/usr/bin/gcc-7 -D CMAKE_CXX_COMPILER=/usr/bin/g++-7
// gcc -O3 -Q --help=optimizers, to list the optimizations enabled
//./toplev.py --long-desc --user mpirun -np 1 /home/diego/Documents/workspace/HPCCG-RHT/cmake-build-debug/HPCCG-current 70 70 40 5 2 1 3
//sudo gedit /proc/sys/kernel/nmi_watchdog
#ifndef HPCCG_1_0_SYNCQUEUE_H
#define HPCCG_1_0_SYNCQUEUE_H


#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
//#include <zconf.h>
#include <math.h>
#include <zconf.h>


// -------- Macros ----------
// a cache line is 64 bytes, int -> 4 bytes, double -> 8 bytes
// so 8 doubles are 64 bytes, 16 doubles are 2 caches lines (for prefetcher)
#define RHT_QUEUE_SIZE 1024 // > 512 make no real diff
#define MIN_PTR_DIST 128 // > 200 makes no real diff
#define ALREADY_CONSUMED -251802.891237
#define GROUP_GRANULARITY 16
#define INLINE inline __attribute__((always_inline))
#define EPSILON 0.000001
#define fequal(a,b) (fabs(a-b) < EPSILON)

typedef struct {
    volatile int deqPtr;
    double padding0[31];
    volatile int enqPtr;
    double padding1[31];
    volatile double *content;
    double padding2[31];
    volatile double volatileValue;
    volatile int checkState;
    double padding3[30];
    // Producer Local, does not make any diff compared to having them outside of the queue
    int nextEnq, localDeq, newLimit, diff;
    double padding4[30];
    // Consumer Local
    double otherValue, thisValue;
}RHT_QUEUE;

#define UNIT 16

typedef struct {
    volatile int deqPtr, deqIter;
    double deqGroupVal;
    unsigned long consumerCount;
    double padding0[29];
    int deqPtrLocal, enqPtrCached;
    unsigned long producerCount;
    double padding1[30];
    volatile int enqPtr, enqIter;
    double enqGroupVal;
    double padding2[30];
    int enqPtrLocal, deqPtrCached;
    double padding3[31];
    volatile double *content;
    double padding4[31];
    volatile double volatileValue;
    volatile int checkState;
    double padding5[30];
    int pResidue, cResidue, nextEnq;
}WANG_QUEUE;

extern RHT_QUEUE globalQueue;
extern WANG_QUEUE wangQueue;

static void Queue_Init() {
    int i = 0;
    globalQueue.content = (double *) (malloc(sizeof(double) * RHT_QUEUE_SIZE));
    for (; i < RHT_QUEUE_SIZE; i++) {
        globalQueue.content[i] = ALREADY_CONSUMED;
    }
    globalQueue.volatileValue = ALREADY_CONSUMED;
    globalQueue.checkState = 1;
    globalQueue.enqPtr = globalQueue.deqPtr = wangQueue.consumerCount = wangQueue.producerCount = 0;
}

static void Wang_Queue_Init() {
    wangQueue.content = (double *) (malloc(sizeof(double) * RHT_QUEUE_SIZE));
    wangQueue.checkState = 1;
    wangQueue.enqPtr = wangQueue.enqPtrLocal = wangQueue.enqPtrCached =
    wangQueue.deqPtr = wangQueue.deqPtrLocal = wangQueue.deqPtrCached =
    wangQueue.deqGroupVal = wangQueue.deqIter = wangQueue.enqGroupVal =
    wangQueue.enqIter = wangQueue.consumerCount = wangQueue.producerCount = 0;
    for (int i; i < RHT_QUEUE_SIZE; i++) {
        wangQueue.content[i] = ALREADY_CONSUMED;
    }
}

static void RHT_Replication_Init() {
#if APPROACH_WANG == 1 || APPROACH_MIX_WANG == 1 || APPROACH_MIX_IMPROVED == 1
    Wang_Queue_Init();
#else
    Queue_Init();
#endif
}

static void RHT_Replication_Finish() {
#if APPROACH_WANG == 1 || APPROACH_MIX_WANG == 1 || APPROACH_MIX_IMPROVED == 1
    if(wangQueue.content)
        free((void *) wangQueue.content);
#else
    if (globalQueue.content)
        free((void *) globalQueue.content);
#endif
}

// compiler option -ffast-math, has a problem with isnan method
static int are_both_nan(double pValue, double cValue){
    /* if(_GLIBCXX_FAST_MATH){ */
    /*     // todo, how to check for nan when the optimization is enabled */
    /*     printf("The -ffast-math is enabled and there is a problem checking for NaN values...\n\n\n"); */
    /*     exit(1); */
    /* } */
    return isnan(pValue) && isnan(cValue);
}

// must be inside {}
#define Report_Soft_Error(consumerValue, producerValue) \
    printf("\n SOFT ERROR DETECTED, Consumer: %f Producer: %f -- PCount: %lu vs CCount: %lu, distance: %lu \n",  \
            consumerValue, producerValue, wangQueue.producerCount, wangQueue.consumerCount, wangQueue.producerCount - wangQueue.consumerCount); \
    exit(1);

#if OUR_IMPROVEMENTS == 1
#define ThreadWait asm("pause");
#else
#define ThreadWait ;
#endif

#if APPROACH_WANG == 1
#define RHT_Produce_Volatile(volValue)                                      \
    wangQueue.pResidue = (UNIT - (wangQueue.enqPtrLocal % UNIT)) % UNIT;    \
    if (wangQueue.pResidue > 0) {                                           \
        wangQueue.enqPtrLocal = (wangQueue.enqPtrLocal + (wangQueue.pResidue - 1)) % RHT_QUEUE_SIZE; \
        Wang_Produce(0);                                                    \
    }                                                                       \
    wangQueue.volatileValue = volValue;                                     \
    wangQueue.checkState = 0;                                               \
    while (wangQueue.checkState == 0); ThreadWait

#define RHT_Consume_Volatile(volValue)                                      \
    while (wangQueue.checkState == 1);  ThreadWait                          \
    if (!fequal(volValue, wangQueue.volatileValue)){                        \
        Report_Soft_Error(volValue, wangQueue.volatileValue)                \
    }                                                                       \
    wangQueue.checkState = 1;                                               \
    wangQueue.cResidue = (UNIT - (wangQueue.deqPtrLocal % UNIT)) % UNIT;    \
    if (wangQueue.cResidue > 0) {                                           \
        wangQueue.deqPtrLocal = (wangQueue.deqPtrLocal + (wangQueue.cResidue - 1)) % RHT_QUEUE_SIZE; \
        Wang_Consume();                                                     \
    }

#elif APPROACH_MIX_WANG == 1 || APPROACH_MIX_IMPROVED == 1
#define RHT_Produce_Volatile(volValue)                        \
    wangQueue.volatileValue = volValue;                       \
    wangQueue.checkState = 0;                                 \
    while (wangQueue.checkState == 0) asm("pause");

#define RHT_Consume_Volatile(volValue)                          \
while (wangQueue.checkState == 1) asm("pause");                 \
    if (!fequal(volValue, wangQueue.volatileValue)){            \
        Report_Soft_Error(volValue, wangQueue.volatileValue)    \
    }                                                           \
    wangQueue.producerCount = wangQueue.consumerCount = 0;      \
    wangQueue.checkState = 1;

#else
#define RHT_Produce_Volatile(volValue)                          \
    globalQueue.volatileValue = volValue;                       \
    globalQueue.checkState = 0;                                 \
    while (globalQueue.checkState == 0) asm("pause");

#define RHT_Consume_Volatile(volValue)                          \
    while (globalQueue.checkState == 1) asm("pause");           \
    if (!fequal(volValue, globalQueue.volatileValue)){          \
        Report_Soft_Error(volValue, globalQueue.volatileValue)  \
    }                                                           \
    globalQueue.checkState = 1;
#endif

static void SetThreadAffinity(int threadId) {
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(threadId, &cpuset);

    /* pin the thread to a core */
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset)) {
        fprintf(stderr, "Thread pinning failed!\n");
        exit(1);
    }
}

//////////// INTERNAL QUEUE METHODS BODY //////////////////

// -------- Already Consumed Approach ----------

static INLINE void AlreadyConsumed_Produce(double value) {
    globalQueue.nextEnq = (globalQueue.enqPtr + 1) % RHT_QUEUE_SIZE;

    while (!fequal(globalQueue.content[globalQueue.nextEnq], ALREADY_CONSUMED)) {
        asm("pause");
    }

    globalQueue.content[globalQueue.enqPtr] = value;
    globalQueue.enqPtr = globalQueue.nextEnq;
}

static INLINE double AlreadyConsumed_Consume() {
    while (fequal(globalQueue.content[globalQueue.deqPtr], ALREADY_CONSUMED)) {
        asm("pause");
    }

    double value = globalQueue.content[globalQueue.deqPtr];
    globalQueue.content[globalQueue.deqPtr] = ALREADY_CONSUMED;
    globalQueue.deqPtr = (globalQueue.deqPtr + 1) % RHT_QUEUE_SIZE;
    return value;
}

static INLINE void AlreadyConsumed_Consume_Check(double currentValue) {
    while (fequal(globalQueue.content[globalQueue.deqPtr], ALREADY_CONSUMED)) {
        asm("pause");
    }

    if (!fequal(globalQueue.content[globalQueue.deqPtr], currentValue)) {
        Report_Soft_Error(globalQueue.content[globalQueue.deqPtr], currentValue)
    }

    globalQueue.content[globalQueue.deqPtr] = ALREADY_CONSUMED;
    globalQueue.deqPtr = (globalQueue.deqPtr + 1) % RHT_QUEUE_SIZE;
}

// -------- Using Pointers Approach ----------

static INLINE void UsingPointers_Produce(double value) {
    globalQueue.nextEnq = (globalQueue.enqPtr + 1) % RHT_QUEUE_SIZE;

    while (fequal(globalQueue.nextEnq, globalQueue.deqPtr)) {
        asm("pause");
    }

    globalQueue.content[globalQueue.enqPtr] = value;
    globalQueue.enqPtr = globalQueue.nextEnq;
}

static INLINE double UsingPointers_Consume() {

    while (fequal(globalQueue.deqPtr, globalQueue.enqPtr)) {
        //asm("pause");
    }

    double value = globalQueue.content[globalQueue.deqPtr];
    globalQueue.deqPtr = (globalQueue.deqPtr + 1) % RHT_QUEUE_SIZE;
    return value;
}

static INLINE void UsingPointers_Consume_Check(double currentValue) {
    while (fequal(globalQueue.deqPtr, globalQueue.enqPtr)) {
        //asm("pause");
    }

    if (!fequal(globalQueue.content[globalQueue.deqPtr], currentValue)) {

        if (are_both_nan(globalQueue.content[globalQueue.deqPtr], currentValue)){
            globalQueue.deqPtr = (globalQueue.deqPtr + 1) % RHT_QUEUE_SIZE;
            return;
        }

        Report_Soft_Error(currentValue, globalQueue.content[globalQueue.deqPtr])
    }

    globalQueue.deqPtr = (globalQueue.deqPtr + 1) % RHT_QUEUE_SIZE;
}

#define produce_move(value)                                  \
    wangQueue.content[wangQueue.enqPtr] = value;                    \
    wangQueue.enqPtr = (wangQueue.enqPtr + 1) % RHT_QUEUE_SIZE;

#define consumer_move(value)                                  \
    wangQueue.content[wangQueue.deqPtr] = ALREADY_CONSUMED;          \
    wangQueue.deqPtr = (wangQueue.deqPtr + 1) % RHT_QUEUE_SIZE;


// -------- Wang Approach from Compiler-Managed Software-based Redundant ... paper ----------

static INLINE void Wang_Produce(double value) {
    wangQueue.content[wangQueue.enqPtrLocal] = value;
    wangQueue.enqPtrLocal = (wangQueue.enqPtrLocal + 1) % RHT_QUEUE_SIZE;

    if(wangQueue.enqPtrLocal % UNIT == 0) {
        // While were at the limit, we update the cached deqPtr
        while(wangQueue.enqPtrLocal == wangQueue.deqPtrCached) {
            wangQueue.deqPtrCached = wangQueue.deqPtr;
            ThreadWait
        }
        wangQueue.enqPtr = wangQueue.enqPtrLocal;
    }
}

static INLINE double Wang_Consume() {
    if(wangQueue.deqPtrLocal % UNIT == 0) {
        wangQueue.deqPtr = wangQueue.deqPtrLocal;
        // While were at the limit, we update the cached enqPtr
        while(wangQueue.deqPtrLocal == wangQueue.enqPtrCached) {
            wangQueue.enqPtrCached = wangQueue.enqPtr;
            ThreadWait
        }
    }

    double data = wangQueue.content[wangQueue.deqPtrLocal];
    wangQueue.deqPtrLocal = (wangQueue.deqPtrLocal + 1) % RHT_QUEUE_SIZE;
    return data;
}

static INLINE void Wang_Consume_Check(double currentValue) {
    if(wangQueue.deqPtrLocal % UNIT == 0) {
        wangQueue.deqPtr = wangQueue.deqPtrLocal;

        // While were at the limit, we update the cached enqPtr
        while(wangQueue.deqPtrLocal == wangQueue.enqPtrCached) {
            wangQueue.enqPtrCached = wangQueue.enqPtr;
            ThreadWait
        }
    }

#if OUR_IMPROVEMENTS == 1
    if (__builtin_expect(fequal(wangQueue.content[wangQueue.deqPtrLocal], currentValue), 1))
#else
    if (fequal(wangQueue.content[wangQueue.deqPtrLocal], currentValue))
#endif
    {
        wangQueue.deqPtrLocal = (wangQueue.deqPtrLocal + 1) % RHT_QUEUE_SIZE;
    } else{
        Report_Soft_Error(currentValue, wangQueue.content[wangQueue.deqPtrLocal])
    }


}

// -------- Mix approach of wang and ours

static INLINE void Mix_Produce(double value) {
//    if(!fequal(wangQueue.content[wangQueue.enqPtr], ALREADY_CONSUMED)){
//        printf("Overwriting values at index %d, wangQueue.producerCount: %ld \n",
//               wangQueue.enqPtr, wangQueue.producerCount);
//        exit(35);
//    }

    produce_move(value)

    // while the next entry is the limit
    if (wangQueue.enqPtr == wangQueue.deqPtrCached) {
        wangQueue.nextEnq = (wangQueue.enqPtr + 1) % RHT_QUEUE_SIZE;
        do {
            // if all are at the limit and the next one is already consumed --> we produce make a whole new round
            if (fequal(wangQueue.content[wangQueue.nextEnq], ALREADY_CONSUMED) &&
            wangQueue.deqPtr == wangQueue.enqPtr){
                wangQueue.deqPtrCached = (wangQueue.deqPtrCached + (RHT_QUEUE_SIZE - 1)) % RHT_QUEUE_SIZE;
            }else{
                asm("pause");
                wangQueue.deqPtrCached = wangQueue.deqPtr;
            }
        } while (wangQueue.enqPtr == wangQueue.deqPtrCached);
    }
}

static INLINE double Mix_Consume() {
    if(fequal(wangQueue.content[wangQueue.deqPtr], ALREADY_CONSUMED)){
        while (fequal(wangQueue.content[wangQueue.deqPtr], ALREADY_CONSUMED)){
            asm("pause");
        }
    }

    double value = wangQueue.content[wangQueue.deqPtr];
    consumer_move()
    return value;
}

static INLINE void Mix_Consume_Check(double trailingValue) {
    if (__builtin_expect(fequal(wangQueue.content[wangQueue.deqPtr], trailingValue), 1)) {
        consumer_move()
    } else {
//        wangQueue.wangQueue.consumerCount++;
        wangQueue.deqPtrCached = (wangQueue.deqPtrCached + (RHT_QUEUE_SIZE - 1)) % RHT_QUEUE_SIZE;
        while (fequal(wangQueue.content[wangQueue.deqPtr], ALREADY_CONSUMED)){
            asm("pause");
        }

        if (__builtin_expect(fequal(wangQueue.content[wangQueue.deqPtr], trailingValue), 1)) {
            consumer_move()
        } else {
            printf("Wrong deqPtr: %d vs enqPtr: %d ... LValue: %f vs TValue: %f equal? %d \n",
                   wangQueue.deqPtr, wangQueue.enqPtr, trailingValue, wangQueue.content[wangQueue.deqPtr],
                   fequal(wangQueue.content[wangQueue.deqPtr], trailingValue));
            Report_Soft_Error(trailingValue, wangQueue.content[wangQueue.deqPtr])
        }
    }
}

// -------- Mix improved, yeah rigth! ----------

static INLINE void Mix_Produce_Improved(double value) {
//    long diff = wangQueue.producerCount - wangQueue.consumerCount;
//    if (diff > RHT_QUEUE_SIZE){
//    //if (wangQueue.producerCount - wangQueue.consumerCount > RHT_QUEUE_SIZE){
//        printf("Overwriting at index %d --> %d, wangQueue.producerCount: %lu vs wangQueue.consumerCount %lu, diff: %lu enq: %d vs deq: %d\n",
//               wangQueue.enqPtr, wangQueue.deqPtrCached, wangQueue.producerCount, wangQueue.consumerCount,
//               diff, wangQueue.enqPtr, wangQueue.deqPtr);
//        if(1)
//            exit(35);
//    }

    wangQueue.content[wangQueue.enqPtr] = value;
    wangQueue.enqPtr = (wangQueue.enqPtr + 1) % RHT_QUEUE_SIZE;

    volatile long distance; // must be long because a diff between unsigned longs can get messy
    while (wangQueue.enqPtr == wangQueue.deqPtrCached) {
        if (wangQueue.producerCount <= wangQueue.consumerCount) {
            // a whole new round
            break;
        }
        // we know producerCount > consumerCount
        distance = wangQueue.producerCount - wangQueue.consumerCount;

        if(distance > MIN_PTR_DIST){
            // proportional wait
            while(distance > 0){
                distance /= 2;
                asm("pause");
            }
        }else {
            wangQueue.deqPtrCached = (int) (wangQueue.deqPtrCached + (RHT_QUEUE_SIZE - distance - 1)) % RHT_QUEUE_SIZE;
        }
    }

    wangQueue.producerCount++;
}

static INLINE double Mix_Consume_Improved() {
    while (wangQueue.consumerCount >= wangQueue.producerCount){
        asm("pause");
    }

    double value = wangQueue.content[wangQueue.deqPtr];
    wangQueue.deqPtr = (wangQueue.deqPtr + 1) % RHT_QUEUE_SIZE, wangQueue.consumerCount++;
//    asm volatile("" ::: "memory");
    return value;
}

static INLINE void Mix_Consume_Check_Improved(double trailingValue) {
    double currentValue = wangQueue.content[wangQueue.deqPtr];
    if (__builtin_expect(fequal(currentValue, trailingValue), 1)) {
        wangQueue.deqPtr = (wangQueue.deqPtr + 1) % RHT_QUEUE_SIZE, wangQueue.consumerCount++;
    } else {
        //wangQueue.deqPtrCached = (wangQueue.deqPtrCached + (RHT_QUEUE_SIZE - 1)) % RHT_QUEUE_SIZE;
        while (wangQueue.consumerCount >= wangQueue.producerCount){
            asm("pause");
        }

        if (__builtin_expect(fequal(wangQueue.content[wangQueue.deqPtr], trailingValue), 1)) {
            wangQueue.deqPtr = (wangQueue.deqPtr + 1) % RHT_QUEUE_SIZE, wangQueue.consumerCount++;
        } else {
            printf("Wrong deqPtr: %d vs enqPtr: %d ... LValue: %f vs TValue: %f equal? %d \n",
                   wangQueue.deqPtr, wangQueue.enqPtr, trailingValue, wangQueue.content[wangQueue.deqPtr],
                   fequal(wangQueue.content[wangQueue.deqPtr], trailingValue));
            Report_Soft_Error(trailingValue, wangQueue.content[wangQueue.deqPtr])
        }
    }
}

// var grouping methods
static INLINE void VG_Consume_Check(double trailingValue) {
    wangQueue.deqGroupVal += trailingValue;
    if (wangQueue.deqIter++ % GROUP_GRANULARITY == 0) {
#if APPROACH_MIX_WANG == 1
        Mix_Consume_Check(wangQueue.deqGroupVal);
#elif APPROACH_MIX_IMPROVED == 1
        Mix_Consume_Check_Improved(wangQueue.deqGroupVal);
#elif APPROACH_WANG == 1
        Wang_Consume_Check(wangQueue.deqGroupVal);
#endif
        wangQueue.deqGroupVal = 0;
    }
}

static INLINE void VG_Produce(double value) {
    wangQueue.enqGroupVal += value;
    if (wangQueue.enqIter++ % GROUP_GRANULARITY == 0) {
#if APPROACH_MIX_WANG == 1
        Mix_Produce(wangQueue.enqGroupVal);
#elif APPROACH_MIX_IMPROVED == 1
        Mix_Produce_Improved(wangQueue.enqGroupVal);
#elif APPROACH_WANG == 1
        Wang_Produce(wangQueue.enqGroupVal);
#endif
        wangQueue.enqGroupVal = 0;
    }
}

//////////// 'PUBLIC' QUEUE METHODS //////////////////
void RHT_Produce(double value);
void RHT_Produce_NoCheck(double value);
void RHT_Consume_Check(double currentValue);
double RHT_Consume();

#endif //HPCCG_1_0_SYNCQUEUE_H
