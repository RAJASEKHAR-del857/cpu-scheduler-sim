#pragma once
#include "process.h"
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────
//  Abstract Scheduler Base
// ─────────────────────────────────────────────────────────────
class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual SchedulerResult run(std::vector<PCB> processes) = 0;
    virtual std::string name() const = 0;

protected:
    void computeStats(SchedulerResult& res);
};

// ─────────────────────────────────────────────────────────────
//  FCFS — First-Come First-Served
// ─────────────────────────────────────────────────────────────
class FCFS : public Scheduler {
public:
    SchedulerResult run(std::vector<PCB> processes) override;
    std::string name() const override { return "FCFS"; }
};

// ─────────────────────────────────────────────────────────────
//  SJF — Shortest Job First (non-preemptive)
// ─────────────────────────────────────────────────────────────
class SJF : public Scheduler {
public:
    SchedulerResult run(std::vector<PCB> processes) override;
    std::string name() const override { return "SJF (Non-Preemptive)"; }
};

// ─────────────────────────────────────────────────────────────
//  SRTF — Shortest Remaining Time First (preemptive SJF)
// ─────────────────────────────────────────────────────────────
class SRTF : public Scheduler {
public:
    SchedulerResult run(std::vector<PCB> processes) override;
    std::string name() const override { return "SRTF (Preemptive SJF)"; }
};

// ─────────────────────────────────────────────────────────────
//  Round Robin
// ─────────────────────────────────────────────────────────────
class RoundRobin : public Scheduler {
    int quantum;
public:
    explicit RoundRobin(int q = 2) : quantum(q) {}
    SchedulerResult run(std::vector<PCB> processes) override;
    std::string name() const override {
        return "Round Robin (Q=" + std::to_string(quantum) + ")";
    }
};

// ─────────────────────────────────────────────────────────────
//  Priority — non-preemptive with aging (starvation prevention)
// ─────────────────────────────────────────────────────────────
class PriorityScheduler : public Scheduler {
    bool preemptive;
    int  aging_threshold;  // increment priority every N ticks
public:
    explicit PriorityScheduler(bool preemptive = false, int aging = 5)
        : preemptive(preemptive), aging_threshold(aging) {}
    SchedulerResult run(std::vector<PCB> processes) override;
    std::string name() const override {
        return preemptive ? "Priority (Preemptive)" : "Priority (Non-Preemptive)";
    }
};

// ─────────────────────────────────────────────────────────────
//  MLFQ — Multi-Level Feedback Queue
// ─────────────────────────────────────────────────────────────
struct MLFQConfig {
    int  num_queues    = 3;
    std::vector<int> quantums = {4, 8, 16};  // per-level quantum
    int  boost_interval = 40;                // starvation prevention
};

class MLFQ : public Scheduler {
    MLFQConfig cfg;
public:
    explicit MLFQ(MLFQConfig c = {}) : cfg(std::move(c)) {}
    SchedulerResult run(std::vector<PCB> processes) override;
    std::string name() const override { return "MLFQ"; }
};
