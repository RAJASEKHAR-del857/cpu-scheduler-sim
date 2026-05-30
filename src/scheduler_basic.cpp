#include "scheduler.h"
#include <algorithm>
#include <numeric>
#include <climits>

// ─────────────────────────────────────────────────────────────
//  Helper: compute aggregate statistics
// ─────────────────────────────────────────────────────────────
void Scheduler::computeStats(SchedulerResult& res) {
    double tw = 0, tt = 0, tr = 0;
    int busy = 0;
    for (const auto& e : res.gantt)
        if (e.pid != -1) busy += (e.end - e.start);

    int makespan = res.gantt.empty() ? 1 : res.gantt.back().end;

    for (const auto& p : res.processes) {
        tw += p.waiting_time;
        tt += p.turnaround_time;
        tr += p.response_time;
    }
    size_t n = res.processes.size();
    res.avg_waiting_time    = n ? tw / n : 0;
    res.avg_turnaround_time = n ? tt / n : 0;
    res.avg_response_time   = n ? tr / n : 0;
    res.cpu_utilization     = makespan ? (double)busy / makespan * 100.0 : 0;
    res.throughput          = makespan ? (double)n / makespan : 0;
}

// ─────────────────────────────────────────────────────────────
//  FCFS
// ─────────────────────────────────────────────────────────────
SchedulerResult FCFS::run(std::vector<PCB> procs) {
    SchedulerResult res;
    std::sort(procs.begin(), procs.end(),
              [](const PCB& a, const PCB& b){ return a.arrival_time < b.arrival_time; });

    int time = 0, ctx = 0;
    for (auto& p : procs) {
        if (time < p.arrival_time) {
            res.gantt.push_back({-1, "IDLE", time, p.arrival_time});
            time = p.arrival_time;
        }
        p.state = ProcessState::RUNNING;
        p.start_time    = time;
        p.response_time = time - p.arrival_time;
        p.finish_time   = time + p.burst_time;
        p.turnaround_time = p.finish_time - p.arrival_time;
        p.waiting_time    = p.turnaround_time - p.burst_time;
        p.state = ProcessState::TERMINATED;

        res.gantt.push_back({p.pid, p.name, time, p.finish_time});
        time = p.finish_time;
        ctx++;
    }
    res.processes = procs;
    res.total_context_switches = ctx;
    computeStats(res);
    return res;
}

// ─────────────────────────────────────────────────────────────
//  SJF (non-preemptive)
// ─────────────────────────────────────────────────────────────
SchedulerResult SJF::run(std::vector<PCB> procs) {
    SchedulerResult res;
    int n = (int)procs.size();
    std::vector<bool> done(n, false);
    int time = 0, completed = 0, ctx = 0;

    while (completed < n) {
        int idx = -1;
        for (int i = 0; i < n; i++) {
            if (!done[i] && procs[i].arrival_time <= time) {
                if (idx == -1 || procs[i].burst_time < procs[idx].burst_time)
                    idx = i;
            }
        }
        if (idx == -1) {
            // find next arrival
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!done[i]) next = std::min(next, procs[i].arrival_time);
            res.gantt.push_back({-1, "IDLE", time, next});
            time = next;
            continue;
        }
        auto& p = procs[idx];
        p.start_time    = time;
        p.response_time = time - p.arrival_time;
        p.finish_time   = time + p.burst_time;
        p.turnaround_time = p.finish_time - p.arrival_time;
        p.waiting_time    = p.turnaround_time - p.burst_time;
        p.state = ProcessState::TERMINATED;
        res.gantt.push_back({p.pid, p.name, time, p.finish_time});
        time = p.finish_time;
        done[idx] = true;
        completed++;
        ctx++;
    }
    res.processes = procs;
    res.total_context_switches = ctx;
    computeStats(res);
    return res;
}

// ─────────────────────────────────────────────────────────────
//  SRTF (preemptive SJF)
// ─────────────────────────────────────────────────────────────
SchedulerResult SRTF::run(std::vector<PCB> procs) {
    SchedulerResult res;
    int n = (int)procs.size();
    std::vector<bool> done(n, false);
    int time = 0, completed = 0, ctx = 0;
    int prev = -1;

    // find makespan
    int max_time = 0;
    for (auto& p : procs) max_time += p.burst_time + p.arrival_time;

    while (completed < n) {
        int idx = -1;
        for (int i = 0; i < n; i++) {
            if (!done[i] && procs[i].arrival_time <= time && procs[i].remaining_time > 0) {
                if (idx == -1 || procs[i].remaining_time < procs[idx].remaining_time)
                    idx = i;
            }
        }
        if (idx == -1) { time++; continue; }

        if (idx != prev) { ctx++; prev = idx; }

        auto& p = procs[idx];
        if (p.start_time == -1) {
            p.start_time    = time;
            p.response_time = time - p.arrival_time;
        }
        // extend or start gantt entry
        if (!res.gantt.empty() && res.gantt.back().pid == p.pid)
            res.gantt.back().end++;
        else
            res.gantt.push_back({p.pid, p.name, time, time + 1});

        p.remaining_time--;
        time++;

        if (p.remaining_time == 0) {
            p.finish_time     = time;
            p.turnaround_time = p.finish_time - p.arrival_time;
            p.waiting_time    = p.turnaround_time - p.burst_time;
            p.state = ProcessState::TERMINATED;
            done[idx] = true;
            completed++;
            prev = -1;
        }
    }
    res.processes = procs;
    res.total_context_switches = ctx;
    computeStats(res);
    return res;
}
