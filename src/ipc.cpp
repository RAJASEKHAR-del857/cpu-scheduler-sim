#include "ipc.h"
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctime>
#include <cerrno>

// ─────────────────────────────────────────────────────────────
//  Shared Buffer init/destroy
// ─────────────────────────────────────────────────────────────
void SharedBuffer::init() {
    sem_init(&mutex,       0, 1);
    sem_init(&empty_slots, 0, SHM_BUFFER_SIZE);
    sem_init(&full_slots,  0, 0);
}
void SharedBuffer::destroy() {
    sem_destroy(&mutex);
    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);
}

// ─────────────────────────────────────────────────────────────
//  Producer-Consumer using POSIX semaphores
// ─────────────────────────────────────────────────────────────
struct PCArgs {
    SharedBuffer*         buf;
    std::vector<PCEvent>* events;
    std::mutex*           ev_mutex;
    int                   id;
    int                   items;
    bool                  is_producer;
};

static void* producer_thread(void* arg) {
    PCArgs* a = (PCArgs*)arg;
    for (int i = 0; i < a->items; i++) {
        sem_wait(&a->buf->empty_slots);
        sem_wait(&a->buf->mutex);

        int val = a->id * 100 + i;
        a->buf->data[a->buf->in] = val;
        a->buf->in = (a->buf->in + 1) % SHM_BUFFER_SIZE;
        a->buf->count++;

        {
            std::lock_guard<std::mutex> lk(*a->ev_mutex);
            a->events->push_back({"P" + std::to_string(a->id),
                                  "produced", val, a->buf->count});
        }
        sem_post(&a->buf->mutex);
        sem_post(&a->buf->full_slots);
        usleep(10000 + (rand() % 20000));
    }
    return nullptr;
}

static void* consumer_thread(void* arg) {
    PCArgs* a = (PCArgs*)arg;
    for (int i = 0; i < a->items; i++) {
        sem_wait(&a->buf->full_slots);
        sem_wait(&a->buf->mutex);

        int val = a->buf->data[a->buf->out];
        a->buf->out = (a->buf->out + 1) % SHM_BUFFER_SIZE;
        a->buf->count--;

        {
            std::lock_guard<std::mutex> lk(*a->ev_mutex);
            a->events->push_back({"C" + std::to_string(a->id),
                                  "consumed", val, a->buf->count});
        }
        sem_post(&a->buf->mutex);
        sem_post(&a->buf->empty_slots);
        usleep(15000 + (rand() % 25000));
    }
    return nullptr;
}

std::vector<PCEvent> runProducerConsumer(ProducerConsumerConfig cfg) {
    SharedBuffer buf;
    buf.init();
    std::vector<PCEvent> events;
    std::mutex ev_mutex;

    int np = cfg.num_producers, nc = cfg.num_consumers;
    std::vector<pthread_t> producers(np), consumers(nc);
    std::vector<PCArgs>    pargs(np),     cargs(nc);

    for (int i = 0; i < np; i++) {
        pargs[i] = {&buf, &events, &ev_mutex, i+1, cfg.items_per_producer, true};
        pthread_create(&producers[i], nullptr, producer_thread, &pargs[i]);
    }
    for (int i = 0; i < nc; i++) {
        // Distribute consumption evenly
        int items = (cfg.items_per_producer * np) / nc
                  + (i < (cfg.items_per_producer * np) % nc ? 1 : 0);
        cargs[i] = {&buf, &events, &ev_mutex, i+1, items, false};
        pthread_create(&consumers[i], nullptr, consumer_thread, &cargs[i]);
    }
    for (int i = 0; i < np; i++) pthread_join(producers[i], nullptr);
    for (int i = 0; i < nc; i++) pthread_join(consumers[i], nullptr);
    buf.destroy();
    return events;
}

// ─────────────────────────────────────────────────────────────
//  Pipe Demo
// ─────────────────────────────────────────────────────────────
PipeResult runPipeDemo(const std::string& message) {
    PipeResult res;
    int fds[2];
    if (pipe(fds) == -1) { res.log.push_back("pipe() failed"); return res; }

    pid_t pid = fork();
    if (pid == 0) {
        // Child: write to pipe
        close(fds[0]);
        write(fds[1], message.c_str(), message.size());
        close(fds[1]);
        _exit(0);
    } else {
        // Parent: read from pipe
        close(fds[1]);
        char buf[1024] = {};
        int n = (int)read(fds[0], buf, sizeof(buf)-1);
        close(fds[0]);
        waitpid(pid, nullptr, 0);
        res.bytes_transferred = n;
        res.log.push_back("Parent (PID " + std::to_string(getpid()) +
                          ") wrote: " + message);
        res.log.push_back("Child (PID " + std::to_string(pid) +
                          ") received (" + std::to_string(n) + " bytes): " +
                          std::string(buf, n));
    }
    return res;
}

// ─────────────────────────────────────────────────────────────
//  Message Queue (POSIX mqueue)
// ─────────────────────────────────────────────────────────────
static const char* MQ_NAME = "/cpu_sim_mq";

struct MQSenderArgs {
    std::vector<MQEvent>* events;
    std::mutex*           mu;
    int                   id;
    int                   msgs;
    mqd_t                 mq;
};

static void* mq_sender(void* arg) {
    MQSenderArgs* a = (MQSenderArgs*)arg;
    for (int i = 0; i < a->msgs; i++) {
        std::string msg = "MSG_" + std::to_string(a->id) + "_" + std::to_string(i);
        mq_send(a->mq, msg.c_str(), msg.size()+1, (unsigned)(a->id));
        std::lock_guard<std::mutex> lk(*a->mu);
        a->events->push_back({"Sender" + std::to_string(a->id),
                               "sent", msg, a->id});
        usleep(5000);
    }
    return nullptr;
}

std::vector<MQEvent> runMessageQueueDemo(int num_senders,
                                          int num_receivers,
                                          int msgs_each) {
    std::vector<MQEvent> events;
    std::mutex mu;

    mq_unlink(MQ_NAME);
    struct mq_attr attr;
    attr.mq_flags   = 0;
    attr.mq_maxmsg  = 16;
    attr.mq_msgsize = 128;
    attr.mq_curmsgs = 0;

    mqd_t mq = mq_open(MQ_NAME, O_CREAT|O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1) {
        events.push_back({"System","error","mq_open failed: " + std::string(strerror(errno)),0});
        return events;
    }

    std::vector<pthread_t>       sender_threads(num_senders);
    std::vector<MQSenderArgs>    sargs(num_senders);
    for (int i = 0; i < num_senders; i++) {
        sargs[i] = {&events, &mu, i+1, msgs_each, mq};
        pthread_create(&sender_threads[i], nullptr, mq_sender, &sargs[i]);
    }
    for (int i = 0; i < num_senders; i++) pthread_join(sender_threads[i], nullptr);

    // Receivers drain the queue
    int total = num_senders * msgs_each;
    for (int r = 0; r < num_receivers; r++) {
        int per = total / num_receivers + (r < total % num_receivers ? 1 : 0);
        for (int j = 0; j < per; j++) {
            char buf[128] = {};
            unsigned prio = 0;
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            ssize_t n = mq_timedreceive(mq, buf, sizeof(buf), &prio, &ts);
            if (n > 0) {
                std::lock_guard<std::mutex> lk(mu);
                events.push_back({"Receiver" + std::to_string(r+1),
                                  "received", std::string(buf), (int)prio});
            }
        }
    }
    mq_close(mq);
    mq_unlink(MQ_NAME);
    return events;
}

// ─────────────────────────────────────────────────────────────
//  Reader-Writer Problem
// ─────────────────────────────────────────────────────────────
struct RWShared {
    int  value        = 42;
    int  reader_count = 0;
    sem_t mutex;           // protects reader_count
    sem_t write_lock;      // mutual exclusion for writers
    std::vector<RWEvent>* events;
    std::mutex*           ev_mu;
};

struct RWArgs { RWShared* sh; int id; int ops; bool is_reader; };

static void* reader_fn(void* arg) {
    RWArgs* a = (RWArgs*)arg;
    for (int i = 0; i < a->ops; i++) {
        sem_wait(&a->sh->mutex);
        a->sh->reader_count++;
        if (a->sh->reader_count == 1) sem_wait(&a->sh->write_lock);
        sem_post(&a->sh->mutex);

        // Read
        {
            std::lock_guard<std::mutex> lk(*a->sh->ev_mu);
            a->sh->events->push_back({"R"+std::to_string(a->id),"read",a->sh->value});
        }
        usleep(8000 + rand()%10000);

        sem_wait(&a->sh->mutex);
        a->sh->reader_count--;
        if (a->sh->reader_count == 0) sem_post(&a->sh->write_lock);
        sem_post(&a->sh->mutex);
        usleep(5000);
    }
    return nullptr;
}

static void* writer_fn(void* arg) {
    RWArgs* a = (RWArgs*)arg;
    for (int i = 0; i < a->ops; i++) {
        sem_wait(&a->sh->write_lock);
        a->sh->value = a->id * 1000 + i;
        {
            std::lock_guard<std::mutex> lk(*a->sh->ev_mu);
            a->sh->events->push_back({"W"+std::to_string(a->id),"write",a->sh->value});
        }
        usleep(12000 + rand()%15000);
        sem_post(&a->sh->write_lock);
        usleep(5000);
    }
    return nullptr;
}

std::vector<RWEvent> runReaderWriter(RWConfig cfg) {
    std::vector<RWEvent> events;
    std::mutex ev_mu;
    RWShared sh;
    sh.events = &events;
    sh.ev_mu  = &ev_mu;
    sem_init(&sh.mutex,      0, 1);
    sem_init(&sh.write_lock, 0, 1);

    std::vector<pthread_t> rts(cfg.num_readers), wts(cfg.num_writers);
    std::vector<RWArgs>    rargs(cfg.num_readers), wargs(cfg.num_writers);

    for (int i = 0; i < cfg.num_readers; i++) {
        rargs[i] = {&sh, i+1, cfg.ops_each, true};
        pthread_create(&rts[i], nullptr, reader_fn, &rargs[i]);
    }
    for (int i = 0; i < cfg.num_writers; i++) {
        wargs[i] = {&sh, i+1, cfg.ops_each, false};
        pthread_create(&wts[i], nullptr, writer_fn, &wargs[i]);
    }
    for (int i = 0; i < cfg.num_readers; i++) pthread_join(rts[i], nullptr);
    for (int i = 0; i < cfg.num_writers; i++) pthread_join(wts[i], nullptr);

    sem_destroy(&sh.mutex);
    sem_destroy(&sh.write_lock);
    return events;
}
