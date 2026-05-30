#pragma once
#include <string>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>
#include <semaphore.h>

// ─────────────────────────────────────────────────────────────
//  Process States
// ─────────────────────────────────────────────────────────────
enum class ProcessState {
    NEW, READY, RUNNING, WAITING, TERMINATED
};

inline const char* stateStr(ProcessState s) {
    switch(s) {
        case ProcessState::NEW:        return "NEW";
        case ProcessState::READY:      return "READY";
        case ProcessState::RUNNING:    return "RUNNING";
        case ProcessState::WAITING:    return "WAITING";
        case ProcessState::TERMINATED: return "TERMINATED";
    }
    return "UNKNOWN";
}

// ─────────────────────────────────────────────────────────────
//  Process Control Block
// ─────────────────────────────────────────────────────────────
struct PCB {
    int   pid;
    std::string name;
    ProcessState state;

    int arrival_time;
    int burst_time;          // total CPU burst needed
    int remaining_time;      // for preemptive schedulers
    int priority;            // lower = higher priority
    int queue_level;         // for MLFQ

    int start_time    = -1;
    int finish_time   = -1;
    int waiting_time  = 0;
    int turnaround_time = 0;
    int response_time = -1;

    int age = 0;             // starvation prevention counter

    PCB(int pid, std::string name, int arrival, int burst, int priority = 0)
        : pid(pid), name(std::move(name)), state(ProcessState::NEW),
          arrival_time(arrival), burst_time(burst),
          remaining_time(burst), priority(priority), queue_level(0) {}
};

// ─────────────────────────────────────────────────────────────
//  Gantt Chart Entry
// ─────────────────────────────────────────────────────────────
struct GanttEntry {
    int pid;          // -1 = idle
    std::string name;
    int start;
    int end;
};

// ─────────────────────────────────────────────────────────────
//  Scheduler Result
// ─────────────────────────────────────────────────────────────
struct SchedulerResult {
    std::vector<GanttEntry> gantt;
    std::vector<PCB>        processes;
    double avg_waiting_time;
    double avg_turnaround_time;
    double avg_response_time;
    double cpu_utilization;
    double throughput;
    int    total_context_switches;
};
