#pragma once
#include <string>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <atomic>

// ─────────────────────────────────────────────────────────────
//  Shared Memory Ring Buffer (Producer-Consumer)
// ─────────────────────────────────────────────────────────────
#define SHM_BUFFER_SIZE 8

struct SharedBuffer {
    int   data[SHM_BUFFER_SIZE];
    int   in  = 0;
    int   out = 0;
    int   count = 0;
    sem_t mutex;
    sem_t empty_slots;
    sem_t full_slots;

    void init();
    void destroy();
};

struct ProducerConsumerConfig {
    int num_producers   = 2;
    int num_consumers   = 2;
    int items_per_producer = 5;
    int buffer_size     = SHM_BUFFER_SIZE;
};

struct PCEvent {
    std::string actor;   // "P1", "C2", etc.
    std::string action;  // "produced", "consumed", "waiting"
    int value;
    int buffer_fill;
};

std::vector<PCEvent> runProducerConsumer(ProducerConsumerConfig cfg);

// ─────────────────────────────────────────────────────────────
//  Pipe — unidirectional data pipe simulation
// ─────────────────────────────────────────────────────────────
struct PipeResult {
    std::vector<std::string> log;
    int bytes_transferred;
};

PipeResult runPipeDemo(const std::string& message);

// ─────────────────────────────────────────────────────────────
//  Message Queue (POSIX mqueue) simulation
// ─────────────────────────────────────────────────────────────
struct MQEvent {
    std::string sender;
    std::string receiver;
    std::string message;
    int priority;
};

std::vector<MQEvent> runMessageQueueDemo(int num_senders = 2,
                                          int num_receivers = 2,
                                          int msgs_each = 3);

// ─────────────────────────────────────────────────────────────
//  Reader-Writer Problem (POSIX threads + semaphores)
// ─────────────────────────────────────────────────────────────
struct RWEvent {
    std::string actor;
    std::string action;  // "read_start","read_end","write_start","write_end","waiting"
    int shared_value;
};

struct RWConfig {
    int num_readers = 4;
    int num_writers = 2;
    int ops_each    = 3;
    bool readers_priority = true;
};

std::vector<RWEvent> runReaderWriter(RWConfig cfg);
