//
// Created by diego on 10/11/17.
//

#include "RHT.h"

RHT_QUEUE globalQueue;

#if APPROACH_WANG == 1 || APPROACH_MIX_WANG == 1 || APPROACH_MIX_IMPROVED == 1
WANG_QUEUE wangQueue;
#endif

void RHT_Produce(double value) {
#if APPROACH_USING_POINTERS == 1
    UsingPointers_Produce(value);
#elif APPROACH_ALREADY_CONSUMED == 1 || APPROACH_CONSUMER_NO_SYNC == 1 // consumer no sync uses alreadyConsume produce method
    AlreadyConsumed_Produce(value);
#elif VAR_GROUPING == 1 && (APPROACH_WANG == 1 || APPROACH_MIX_WANG == 1 || APPROACH_MIX_IMPROVED == 1) // var grouping for wang and mixes
    VG_Produce(value);
#elif APPROACH_WANG == 1
    Wang_Produce(value);
#elif APPROACH_MIX_WANG == 1
    Mix_Produce(value);
#elif APPROACH_MIX_IMPROVED == 1
    Mix_Produce_Improved(value);
#else
    printf("NO APPROACH SPECIFIED\n");
    exit(1);
#endif
}

// directly pushes a new value in the queue (regardless of var grouping)
void RHT_Produce_NoCheck(double value) {
#if APPROACH_MIX_WANG == 1
    Mix_Produce(value);
#elif APPROACH_MIX_IMPROVED == 1
    Mix_Produce_Improved(value);
#elif APPROACH_WANG == 1
    Wang_Produce(value);
#else
    RHT_Produce(value);
#endif
}

void RHT_Consume_Check(double currentValue) {
#if APPROACH_USING_POINTERS == 1
    UsingPointers_Consume_Check(currentValue);
#elif APPROACH_ALREADY_CONSUMED == 1
    AlreadyConsumed_Consume_Check(currentValue);
#elif VAR_GROUPING == 1 && (APPROACH_WANG == 1 || APPROACH_MIX_WANG == 1 || APPROACH_MIX_IMPROVED == 1)  // var grouping for wang and mix
    VG_Consume_Check(currentValue);
#elif APPROACH_WANG == 1
    Wang_Consume_Check(currentValue);
#elif APPROACH_MIX_WANG == 1
    Mix_Consume_Check(currentValue);
#elif APPROACH_MIX_IMPROVED == 1
    Mix_Consume_Check_Improved(currentValue);
#else
    printf("NO APPROACH SPECIFIED\n");
    exit(1);
#endif
}

double RHT_Consume() {
#if APPROACH_USING_POINTERS == 1
    return UsingPointers_Consume();
#elif APPROACH_ALREADY_CONSUMED == 1
    return AlreadyConsumed_Consume();
#elif APPROACH_WANG == 1
    return Wang_Consume();
#elif APPROACH_MIX_WANG == 1
    return Mix_Consume();
#elif APPROACH_MIX_IMPROVED == 1
    return Mix_Consume_Improved();
#else
    printf("NO APPROACH SPECIFIED\n");
    exit(1);
#endif

}
