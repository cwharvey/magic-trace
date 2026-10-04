#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

#define NOINLINE __attribute__((noinline))

/* 
 * Emits a ptwrite instruction.
 * We use an explicit inline assembly instruction.
 * To compile this, your compiler/assembler needs to support the ptwrite instruction
 * (e.g., using -mptwrite on gcc/clang).
 */
static inline void emit_ptwrite(uint8_t queue_id, uint64_t depth) {
    uint64_t payload = ((uint64_t)queue_id << 56) | (depth & 0x00FFFFFFFFFFFFFFULL);
    __asm__ volatile ("ptwrite %0" : : "r" (payload));
}

#define NUM_QUEUES 4
_Atomic uint64_t queues[NUM_QUEUES];

NOINLINE void process_item(int x) {
    // Bogus loop to prevent inlining and create some work
    volatile int y = 0;
    for(int i = 0; i < 50; i++) {
        y += x * i;
    }
}

NOINLINE void queue_push(uint8_t queue_id) {
    uint64_t depth = atomic_fetch_add_explicit(&queues[queue_id], 1, memory_order_relaxed) + 1;
    emit_ptwrite(queue_id, depth);
    process_item((int)depth);
}

NOINLINE void queue_pop(uint8_t queue_id) {
    uint64_t depth = atomic_fetch_sub_explicit(&queues[queue_id], 1, memory_order_relaxed) - 1;
    emit_ptwrite(queue_id, depth);
    process_item((int)depth);
}

void* producer_thread(void* arg) {
    (void)arg;
    for (int i = 0; i < 50000; i++) {
        uint8_t qid = rand() % NUM_QUEUES;
        queue_push(qid);
    }
    return NULL;
}

void* consumer_thread(void* arg) {
    (void)arg;
    for (int i = 0; i < 50000; i++) {
        uint8_t qid = rand() % NUM_QUEUES;
        queue_pop(qid);
    }
    return NULL;
}

int main() {
    pthread_t producers[2], consumers[2];
    
    printf("Starting ptwrite queue simulation...\n");
    
    for(int i = 0; i < 2; i++) {
        pthread_create(&producers[i], NULL, producer_thread, NULL);
        pthread_create(&consumers[i], NULL, consumer_thread, NULL);
    }
    
    for(int i = 0; i < 2; i++) {
        pthread_join(producers[i], NULL);
        pthread_join(consumers[i], NULL);
    }
    
    printf("Done.\n");
    return 0;
}
