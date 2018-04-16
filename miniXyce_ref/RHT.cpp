//
// Created by diego on 10/11/17.
//

#include "RHT.h"

RHT_QUEUE globalQueue;

long producerCount;
long consumerCount;

int wait_var;
double wait_calc;

double groupVarProducer;
double groupVarConsumer;
int groupIncompleteConsumer;
int groupIncompleteProducer;
__thread long iterCountProducer;
__thread long iterCountConsumer;

void RHT_Produce_Secure(double value) {
#if APPROACH_USING_POINTERS == 1
    UsingPointers_Produce(value);
#elif APPROACH_ALREADY_CONSUMED == 1
    AlreadyConsumed_Produce(value);
#elif APPROACH_NEW_LIMIT == 1
    AlreadyConsumed_Produce(value);
#elif APPROACH_WRITE_INVERTED_NEW_LIMIT == 1
    WriteInverted_Produce_Secure(value);
#else
    printf("NO APPROACH SPECIFIED\n");
    exit(1);
#endif
}

void RHT_Produce(double value) {
#if APPROACH_USING_POINTERS == 1
    UsingPointers_Produce(value);
#elif APPROACH_ALREADY_CONSUMED == 1
    AlreadyConsumed_Produce(value);
#elif APPROACH_NEW_LIMIT == 1
    NewLimit_Produce(value);
#elif APPROACH_WRITE_INVERTED_NEW_LIMIT == 1
    WriteInvertedNewLimit_Produce(value);
#elif APPROACH_MOODY_CAMEL == 1
    MoodyCamel_Produce(value);
#else
    printf("NO APPROACH SPECIFIED\n");
    exit(1);
#endif
}

void RHT_Consume_Check(double currentValue) {
#if APPROACH_USING_POINTERS == 1
    UsingPointers_Consume_Check(currentValue);
#elif APPROACH_ALREADY_CONSUMED == 1 || APPROACH_NEW_LIMIT == 1 || APPROACH_WRITE_INVERTED_NEW_LIMIT == 1
    AlreadyConsumed_Consume_Check(currentValue);
#elif APPROACH_MOODY_CAMEL == 1
    MoodyCamel_Consume_Check(currentValue);
#else
    printf("NO APPROACH SPECIFIED\n");
    exit(1);
#endif
}

double RHT_Consume() {
#if APPROACH_USING_POINTERS == 1
    return UsingPointers_Consume();
#elif APPROACH_ALREADY_CONSUMED == 1 || APPROACH_NEW_LIMIT == 1 || APPROACH_WRITE_INVERTED_NEW_LIMIT == 1
    return AlreadyConsumed_Consume();
#elif APPROACH_MOODY_CAMEL == 1
    return MoodyCamel_Consume();
#else
    printf("NO APPROACH SPECIFIED\n");
    exit(1);
#endif

}
