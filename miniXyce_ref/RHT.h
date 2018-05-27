//
// Created by diego on 30/10/17.
//

// -D CMAKE_C_COMPILER=/usr/bin/clang-5.0 -D CMAKE_CXX_COMPILER=/usr/bin/clang++-5.0
// -D CMAKE_C_COMPILER=/usr/bin/gcc-7 -D CMAKE_CXX_COMPILER=/usr/bin/g++-7
// gcc -O3 -Q --help=optimizers, to list the optimizations enabled
//./toplev.py --long-desc --user mpirun -np 1 /home/diego/Documents/workspace/HPCCG-RHT/cmake-build-debug/HPCCG_RHT 70 70 40 1 1 3

#ifndef HPCCG_1_0_SYNCQUEUE_H
#define HPCCG_1_0_SYNCQUEUE_H


#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
//#include <zconf.h>
#include <math.h>


// -------- Macros ----------
// a cache line is 64 bytes, int -> 4 bytes, double -> 8 bytes
// so 8 doubles are 64 bytes, 16 doubles are 2 caches lines (for prefetcher)
#define CACHE_LINE_SIZE 16
#define RHT_QUEUE_SIZE 512 // > 512 make no real diff
#define MIN_PTR_DIST 200 // > 200 makes no real diff
#define ALREADY_CONSUMED -251802.89123
#define GROUP_GRANULARITY 8
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

#define UNIT 8
typedef struct {
    volatile int deqPtr;
    double padding0[31];
    volatile int deqPtrDB, enqPtrLS;
    double padding1[31];
    volatile int enqPtr;
    double padding2[31];
    volatile int enqPtrDB, deqPtrLS;
    double padding3[31];
    volatile double *content;
    double padding4[31];
    volatile double volatileValue;
    volatile int checkState;
    double padding5[30];
    int pResidue, cResidue;
}WANG_QUEUE;


extern int wait_var;
extern double wait_calc;

extern RHT_QUEUE globalQueue;
extern WANG_QUEUE wangQueue;

extern double groupVarProducer;
extern double groupVarConsumer;
extern int groupIncompleteConsumer;
extern int groupIncompleteProducer;
extern __thread long iterCountProducer;
extern __thread long iterCountConsumer;

static pthread_t **consumerThreads;
static int consumerThreadCount;

extern long producerCount;
extern long consumerCount;

// compiler option -ffast-math, has a problem with isnan method
static int are_both_nan(double pValue, double cValue){
    /* if(_GLIBCXX_FAST_MATH){ */
    /*     // todo, how to check for nan when the optimization is enabled */
    /*     printf("The -ffast-math is enabled and there is a problem checking for NaN values...\n\n\n"); */
    /*     exit(1); */
    /* } */
    return isnan(pValue) && isnan(cValue);
}

/// ((RHT_QUEUE_SIZE-globalQueue.enqPtr + globalQueue.localDeq) % RHT_QUEUE_SIZE)-1; this would be faster and
/// with the -1 we make sure that the producer never really catches up to the consumer, but it might still happen
/// the other way around,

/// Another thing that we may try is inspired on wait-free programming from moodyCamel queue: the code is set up such
/// that each thread can always advance regardless of what the other is doing, when the queue is full the producer
/// allocates more memory

#define calc_new_distance(waitValue)                                            \
    waitValue = (RHT_QUEUE_SIZE-(globalQueue.enqPtr+1) + globalQueue.deqPtr) % RHT_QUEUE_SIZE;

#define calc_new_distance1(waitValue)                                            \
    globalQueue.localDeq = globalQueue.deqPtr;                                  \
    waitValue = (globalQueue.enqPtr >= globalQueue.localDeq ?                   \
                RHT_QUEUE_SIZE - globalQueue.enqPtr + globalQueue.localDeq:   \
                globalQueue.localDeq - globalQueue.enqPtr)-1;


#define write_move_normal(value)                                        \
    globalQueue.content[globalQueue.enqPtr] = value;                    \
    globalQueue.enqPtr = (globalQueue.enqPtr + 1) % RHT_QUEUE_SIZE;

#define write_move_inverted(value)                                      \
    globalQueue.nextEnq = (globalQueue.enqPtr + 1) % RHT_QUEUE_SIZE;    \
    globalQueue.content[globalQueue.nextEnq] = ALREADY_CONSUMED;        \
    /*asm volatile("" ::: "memory");*/                                  \
    globalQueue.content[globalQueue.enqPtr] = value;                    \
    globalQueue.enqPtr = globalQueue.nextEnq;

#if APPROACH_USING_POINTERS == 1 || APPROACH_ALREADY_CONSUMED == 1 || APPROACH_WANG == 1 || APPROACH_CONSUMER_NO_SYNC == 1
#define write_move(value) RHT_Produce(value);

#elif APPROACH_WRITE_INVERTED_NEW_LIMIT == 1
#define write_move(value) write_move_inverted(value)

#elif APPROACH_NEW_LIMIT == 1
#define write_move(value) write_move_normal(value)

#endif

#if VAR_GROUPING == 1
#define calc_write_move(iterator, operation, value)     \
    operation;                                          \
    groupVarProducer += value;                          \
    if(iterator % GROUP_GRANULARITY == 0){              \
        write_move(groupVarProducer)                    \
        groupVarProducer = 0;                           \
    }
#else
#define calc_write_move(iterator, operation, value)     \
    operation;                                          \
    write_move(value)
#endif


#define wait_for_thread() asm("pause"); //asm("pause");


#define wait_for_thread1()\
    for(wait_var = wait_calc = 0; wait_var < 1000; wait_calc += (wait_var++ % 10));

// waits until the distance between pointers is > MIN_PTR_DIST
#define wait_enough_distance(waitValue)         \
    calc_new_distance(waitValue)                \
    if(waitValue < MIN_PTR_DIST){               \
        /*producerCount++;*/                                                            \
        do{                                     \
            wait_for_thread()                   \
            calc_new_distance(waitValue)        \
        }while(waitValue < MIN_PTR_DIST);       \
    }

// waits until the nextEnqIndex = enq+MIN_PTR_DIST, is ALREADY_CONSUMED, only when the consumer writes
#define wait_next_enqIndex(waitValue)                                                   \
    waitValue = MIN_PTR_DIST;                                                           \
    globalQueue.nextEnq = (globalQueue.enqPtr + MIN_PTR_DIST) % RHT_QUEUE_SIZE;         \
    if(!fequal(ALREADY_CONSUMED, globalQueue.content[globalQueue.nextEnq])){            \
        /*producerCount++;*/                                                            \
        do{                                                                             \
            wait_for_thread()                                                           \
        }while(!fequal(ALREADY_CONSUMED, globalQueue.content[globalQueue.nextEnq]));    \
    }

#if APPROACH_NEW_ENQ_INDEX == 1
#define wait_for_consumer(waitValue) wait_next_enqIndex(waitValue)
#else
#define wait_for_consumer(waitValue) wait_enough_distance(waitValue)
#endif

// try with proportional wait
#define replicate_loop_producer1(numIters, iterator, value, operation)   \
    wait_for_consumer(globalQueue.newLimit)                             \
    iterator = 0;                                                       \
    while (globalQueue.newLimit < numIters) {                           \
        for (; iterator < globalQueue.newLimit; iterator++){            \
            if (iterator % 50 == 0) asm("pause");                      \
            calc_and_move(operation, value)                             \
        }                                                               \
        wait_for_consumer(globalQueue.diff)                             \
        globalQueue.newLimit += globalQueue.diff;                       \
    }                                                                   \
    for (; iterator < numIters; iterator++){                            \
        calc_and_move(operation, value)                                 \
    }

// works for either forward or backward 'for' loops
#if APPROACH_WRITE_INVERTED_NEW_LIMIT == 1 || APPROACH_NEW_LIMIT == 1
#define replicate_loop_work(numIters, iterator, iterOp, value, operation)  \
    wait_for_consumer(globalQueue.diff)                                         \
    while (globalQueue.diff < numIters) {                                       \
        numIters -= globalQueue.diff;                                           \
        for (; globalQueue.diff-- > 0; iterOp){                                 \
            calc_write_move(iterator, operation, value)                         \
        }                                                                       \
        wait_for_consumer(globalQueue.diff)                                     \
    }                                                                           \
    for (; numIters-- > 0; iterOp){                                             \
        calc_write_move(iterator, operation, value)                             \
    }
#else // we simply re-construct the loop
#define replicate_loop_work(numIters, iterator, iterOp, value, operation)   \
    for(; numIters-- > 0; iterOp){                                          \
        calc_write_move(iterator, operation, value)                         \
    }
#endif

#if VAR_GROUPING == 1
#define replicate_loop_producer(sIndex, fIndex, iterator, iterOp, value, operation) \
    iterCountProducer = fIndex - sIndex;                                            \
    groupVarProducer = 0;                                                           \
    groupIncompleteProducer = iterCountProducer % GROUP_GRANULARITY;                \
    replicate_loop_work(iterCountProducer, iterator, iterOp, value, operation)      \
    if (groupIncompleteProducer) {                                                  \
        write_move(groupVarProducer)                                                \
    }
#else
#define replicate_loop_producer(sIndex, fIndex, iterator, iterOp, value, operation) \
    iterCountProducer = fIndex - sIndex;                                            \
    replicate_loop_work(iterCountProducer, iterator, iterOp, value, operation)
#endif


#if VAR_GROUPING == 1
#define replicate_loop_consumer(sIndex, fIndex, iterator, iterOp, value, operation) \
    iterCountConsumer = fIndex - sIndex;                                            \
    groupIncompleteConsumer = iterCountConsumer % GROUP_GRANULARITY;                \
    for(groupVarConsumer = 0; iterCountConsumer-- > 0; iterOp){                     \
        operation;                                                                  \
        groupVarConsumer += value;                                                  \
        if(iterator % GROUP_GRANULARITY == 0){                                      \
            RHT_Consume_Check(groupVarConsumer);                                    \
            groupVarConsumer = 0;                                                   \
        }                                                                           \
    }                                                                               \
    if (groupIncompleteConsumer) RHT_Consume_Check(groupVarConsumer);
#else // we simply re construct the loop
#define replicate_loop_consumer(sIndex, fIndex, iterator, iterOp, value, operation) \
    iterCountConsumer = fIndex - sIndex;                                            \
    for(; iterCountConsumer-- > 0; iterOp){                                         \
        operation;                                                                  \
        RHT_Consume_Check(value);                                                   \
    }
#endif

// must be inside {}
#define Report_Soft_Error(consumerValue, producerValue) \
    printf("\n SOFT ERROR DETECTED, Consumer: %f Producer: %f -- PCount: %ld , CCount: %ld\n",  \
            consumerValue, producerValue, producerCount, consumerCount); \
    exit(1);

#if APPROACH_WANG == 1
#define RHT_Produce_Volatile(volValue)                        \
    wangQueue.pResidue = UNIT - (wangQueue.enqPtrDB % UNIT);            \
    while(wangQueue.pResidue-- > 0) Wang_Produce(0);                  \
    wangQueue.volatileValue = volValue;                       \
    wangQueue.checkState = 0;\
    while (wangQueue.checkState == 0); //asm("pause");
#else
#define RHT_Produce_Volatile(volValue)                          \
    globalQueue.volatileValue = volValue;                       \
    globalQueue.checkState = 0;                                 \
    while (globalQueue.checkState == 0) asm("pause");
#endif

#if APPROACH_WANG == 1
#define RHT_Consume_Volatile(volValue)                        \
    while (wangQueue.checkState == 1);  /*asm("pause");*/           \
    if (!fequal(volValue, wangQueue.volatileValue)){          \
        Report_Soft_Error(volValue, wangQueue.volatileValue)  \
    }                                                         \
    wangQueue.checkState = 1;                               \
    wangQueue.cResidue = UNIT - (wangQueue.deqPtrDB % UNIT);            \
    while(wangQueue.cResidue-- > 0) Wang_Consume();
#else
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

static void Queue_Init() {
    int i = 0;
    globalQueue.content = (double *) (malloc(sizeof(double) * RHT_QUEUE_SIZE));
    for (; i < RHT_QUEUE_SIZE; i++) {
        globalQueue.content[i] = ALREADY_CONSUMED;
    }
    globalQueue.volatileValue = ALREADY_CONSUMED;
    globalQueue.checkState = 1;
    globalQueue.enqPtr = globalQueue.deqPtr = consumerCount = producerCount = 0;
}

static void Wang_Queue_Init() {
    wangQueue.content = (double *) (malloc(sizeof(double) * RHT_QUEUE_SIZE));
    wangQueue.checkState = 1;
    wangQueue.enqPtr = wangQueue.enqPtrDB = wangQueue.enqPtrLS =
    wangQueue.deqPtr = wangQueue.deqPtrDB = wangQueue.deqPtrLS = 0;
}

static void createConsumerThreads(int numThreads) {
    int i;

    consumerThreadCount = numThreads;
    //consumerThreads = (pthread_t **) malloc(sizeof(pthread_t *) * consumerThreadCount);

    //for (i = 0; i < consumerThreadCount; i++)
    //consumerThreads[i] = (pthread_t *) malloc(sizeof(pthread_t));
}

static void RHT_Replication_Init(int numThreads) {
    createConsumerThreads(numThreads);
#if APPROACH_WANG == 1
    Wang_Queue_Init();
#else
    Queue_Init();
#endif
}

static void RHT_Replication_Finish() {
#if APPROACH_WANG == 1
    if(wangQueue.content)
        free((void *) wangQueue.content);
#else
    if (globalQueue.content)
        free((void *) globalQueue.content);
#endif

    //int i = 0;
//    for (i = 0; i < consumerThreadCount; i++)
//        free(consumerThreads[i]);
//    free(consumerThreads);
}

//////////// INTERNAL QUEUE METHODS BODY //////////////////

#if APPROACH_WRITE_INVERTED_NEW_LIMIT == 1
#define consumer_move_next()                                        \
    globalQueue.deqPtr = (globalQueue.deqPtr + 1) % RHT_QUEUE_SIZE;

#else
#define consumer_move_next()                                        \
    globalQueue.content[globalQueue.deqPtr] = ALREADY_CONSUMED;     \
    globalQueue.deqPtr = (globalQueue.deqPtr + 1) % RHT_QUEUE_SIZE;
#endif

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
    consumer_move_next()
    return value;
}

static INLINE void AlreadyConsumed_Consume_Check(double currentValue) {
    while (fequal(globalQueue.content[globalQueue.deqPtr], ALREADY_CONSUMED)) {
        asm("pause");
    }

    if (!fequal(globalQueue.content[globalQueue.deqPtr], currentValue)) {
        Report_Soft_Error(globalQueue.content[globalQueue.deqPtr], currentValue)
    }

    consumer_move_next()
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


// -------- Wang Approach from Compiler-Managed Software-based Redundant ... paper ----------

static INLINE void Wang_Produce(double value) {
    wangQueue.content[wangQueue.enqPtrDB] = value;
    wangQueue.enqPtrDB = (wangQueue.enqPtrDB + 1) % RHT_QUEUE_SIZE;

    if(wangQueue.enqPtrDB % UNIT == 0) {
        while(wangQueue.enqPtrDB == wangQueue.deqPtrLS) {
            wangQueue.deqPtrLS = wangQueue.deqPtr;
            //asm("pause");
        }
        wangQueue.enqPtr = wangQueue.enqPtrDB;
    }
}

static INLINE double Wang_Consume() {
    if(wangQueue.deqPtrDB % UNIT == 0) {
        wangQueue.deqPtr = wangQueue.deqPtrDB;
        while(wangQueue.deqPtrDB == wangQueue.enqPtrLS) {
            wangQueue.enqPtrLS = wangQueue.enqPtr;
            //asm("pause");
        }
    }

    double data = wangQueue.content[wangQueue.deqPtrDB];
    wangQueue.deqPtrDB = (wangQueue.deqPtrDB + 1) % RHT_QUEUE_SIZE;
    return data;
}

static INLINE void Wang_Consume_Check(double currentValue) {
    if(wangQueue.deqPtrDB % UNIT == 0) {
        wangQueue.deqPtr = wangQueue.deqPtrDB;

        while(wangQueue.deqPtrDB == wangQueue.enqPtrLS) {
            wangQueue.enqPtrLS = wangQueue.enqPtr;
            //asm("pause");
        }
    }

    if (!fequal(wangQueue.content[wangQueue.deqPtrDB], currentValue)) {
        Report_Soft_Error(currentValue, wangQueue.content[wangQueue.deqPtrDB])
    }

    wangQueue.deqPtrDB = (wangQueue.deqPtrDB + 1) % RHT_QUEUE_SIZE;
}


// -------- NoSyncConsumer Approach, reducing sync operations in the consumer  ----------

static INLINE double NoSyncConsumer_Consume() {
    double value = globalQueue.content[globalQueue.deqPtr];

#if BRANCH_HINT == 1
    if (__builtin_expect(fequal(value, ALREADY_CONSUMED), 0))
#else
    if (fequal(value, ALREADY_CONSUMED))
#endif
    {
#if COUNT_QUEUE_DESYNC == 1
        consumerCount++;
#endif
        do {
            asm("pause");
        } while (fequal(globalQueue.content[globalQueue.deqPtr], ALREADY_CONSUMED));
        value = globalQueue.content[globalQueue.deqPtr];
    }

    consumer_move_next()
    return value;
}

static INLINE void NoSyncConsumer_Consume_Check(double currentValue) {
    globalQueue.otherValue = globalQueue.content[globalQueue.deqPtr];

#if BRANCH_HINT == 1
    if (__builtin_expect(fequal(currentValue, globalQueue.otherValue), 1))
#else
        if(fequal(currentValue, globalQueue.otherValue))
#endif
    {
        consumer_move_next()
        return;
    }

    // probably des-sync of the queue
#if BRANCH_HINT == 1
    if (__builtin_expect(fequal(globalQueue.otherValue, ALREADY_CONSUMED), 1))
#else
    if(fequal(globalQueue.otherValue, ALREADY_CONSUMED))
#endif
    {
#if COUNT_QUEUE_DESYNC == 1
        consumerCount++;
#endif
        do asm("pause"); while (fequal(globalQueue.content[globalQueue.deqPtr], ALREADY_CONSUMED));
        globalQueue.otherValue = globalQueue.content[globalQueue.deqPtr];

#if BRANCH_HINT == 1
        if (__builtin_expect(fequal(currentValue, globalQueue.otherValue), 1))
#else
            if (fequal(currentValue, globalQueue.otherValue))
#endif
        {
            consumer_move_next()
            return;
        }
    }

    if (are_both_nan(currentValue, globalQueue.otherValue)){
        consumer_move_next()
        return;
    }

    Report_Soft_Error(currentValue, globalQueue.otherValue)
}

// -------- Write Inverted New Limit Approach ----------

static INLINE void WriteInverted_Produce_Secure(double value){
    calc_new_distance(globalQueue.newLimit)

    while(globalQueue.newLimit < 8){
        asm("pause");
        calc_new_distance(globalQueue.newLimit)
    }

    write_move_inverted(value)
}

//////////// 'PUBLIC' QUEUE METHODS //////////////////
void RHT_Produce(double value);
void RHT_Consume_Check(double currentValue);
double RHT_Consume();

#endif //HPCCG_1_0_SYNCQUEUE_H
