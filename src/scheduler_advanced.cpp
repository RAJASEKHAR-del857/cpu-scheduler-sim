#include "scheduler.h"
#include <algorithm>
#include <queue>
#include <climits>

// ─────────────────────────────────────────────────────────────
//  Round Robin
// ─────────────────────────────────────────────────────────────
SchedulerResult RoundRobin::run(std::vector<PCB> procs) {
    SchedulerResult res;
    int n = (int)procs.size();
    std::sort(procs.begin(), procs.end(),
              [](const PCB& a, const PCB& b){ return a.arrival_time < b.arrival_time; });

    std::queue<int> rq;
    std::vector<bool> in_queue(n, false);
    int time = 0, completed = 0, ctx = 0;
    int idx = 0;

    // Enqueue all arriving at t=0
    while (idx < n && procs[idx].arrival_time <= time) {
        rq.push(idx++);
        in_queue[idx-1] = true;
    }

    while (completed < n) {
        if (rq.empty()) {
            if (idx < n) {
                res.gantt.push_back({-1,"IDLE",time,procs[idx].arrival_time});
                time = procs[idx].arrival_time;
                rq.push(idx); in_queue[idx] = true; idx++;
            }
            continue;
        }
        int i = rq.front(); rq.pop();
        auto& p = procs[i];

        if (p.start_time == -1) {
            p.start_time    = time;
            p.response_time = time - p.arrival_time;
        }

        int run_for = std::min(quantum, p.remaining_time);
        res.gantt.push_back({p.pid, p.name, time, time + run_for});
        p.remaining_time -= run_for;
        time += run_for;
        ctx++;

        // Enqueue newly arrived processes
        while (idx < n && procs[idx].arrival_time <= time) {
            rq.push(idx); in_queue[idx] = true; idx++;
        }

        if (p.remaining_time == 0) {
            p.finish_time     = time;
            p.turnaround_time = p.finish_time - p.arrival_time;
            p.waiting_time    = p.turnaround_time - p.burst_time;
            p.state = ProcessState::TERMINATED;
            completed++;
        } else {
            rq.push(i);
        }
    }
    res.processes = procs;
    res.total_context_switches = ctx;

    // Compute stats via base
    {
        double tw=0,tt=0,tr=0; int busy=0;
        for (auto& e : res.gantt) if (e.pid!=-1) busy+=(e.end-e.start);
        int mk = res.gantt.empty()?1:res.gantt.back().end;
        for (auto& p2 : res.processes){tw+=p2.waiting_time;tt+=p2.turnaround_time;tr+=p2.response_time;}
        int sz=(int)res.processes.size();
        res.avg_waiting_time=sz?tw/sz:0;
        res.avg_turnaround_time=sz?tt/sz:0;
        res.avg_response_time=sz?tr/sz:0;
        res.cpu_utilization=mk?(double)busy/mk*100:0;
        res.throughput=mk?(double)sz/mk:0;
    }
    return res;
}

// ─────────────────────────────────────────────────────────────
//  Priority Scheduler (non-preemptive & preemptive) with aging
// ─────────────────────────────────────────────────────────────
SchedulerResult PriorityScheduler::run(std::vector<PCB> procs) {
    SchedulerResult res;
    int n = (int)procs.size();
    std::vector<bool> done(n, false);
    int time=0, completed=0, ctx=0, prev=-1;

    auto pickBest = [&]() -> int {
        int best = -1;
        for (int i=0;i<n;i++) {
            if (!done[i] && procs[i].arrival_time<=time && procs[i].remaining_time>0) {
                if (best==-1 || procs[i].priority < procs[best].priority)
                    best = i;
            }
        }
        return best;
    };

    if (!preemptive) {
        while (completed < n) {
            int idx = pickBest();
            if (idx == -1) {
                int next = INT_MAX;
                for (int i=0;i<n;i++) if (!done[i]) next=std::min(next,procs[i].arrival_time);
                res.gantt.push_back({-1,"IDLE",time,next}); time=next; continue;
            }
            auto& p = procs[idx];
            p.start_time    = time;
            p.response_time = time - p.arrival_time;
            p.finish_time   = time + p.burst_time;
            p.turnaround_time = p.finish_time - p.arrival_time;
            p.waiting_time    = p.turnaround_time - p.burst_time;
            p.state = ProcessState::TERMINATED;
            res.gantt.push_back({p.pid,p.name,time,p.finish_time});
            time = p.finish_time; done[idx]=true; completed++; ctx++;

            // Aging: boost waiting processes
            for (int i=0;i<n;i++) {
                if (!done[i] && procs[i].arrival_time<=time) {
                    procs[i].age++;
                    if (procs[i].age >= aging_threshold) {
                        procs[i].priority = std::max(0, procs[i].priority-1);
                        procs[i].age = 0;
                    }
                }
            }
        }
    } else {
        // Preemptive priority
        while (completed < n) {
            int idx = pickBest();
            if (idx==-1) { time++; continue; }
            if (idx!=prev) { ctx++; prev=idx; }
            auto& p = procs[idx];
            if (p.start_time==-1){p.start_time=time;p.response_time=time-p.arrival_time;}
            if (!res.gantt.empty() && res.gantt.back().pid==p.pid)
                res.gantt.back().end++;
            else
                res.gantt.push_back({p.pid,p.name,time,time+1});
            p.remaining_time--; time++;

            // Aging
            for (int i=0;i<n;i++){
                if (!done[i]&&i!=idx&&procs[i].arrival_time<=time){
                    procs[i].age++;
                    if(procs[i].age>=aging_threshold){
                        procs[i].priority=std::max(0,procs[i].priority-1);
                        procs[i].age=0;
                    }
                }
            }
            if (p.remaining_time==0){
                p.finish_time=time;
                p.turnaround_time=p.finish_time-p.arrival_time;
                p.waiting_time=p.turnaround_time-p.burst_time;
                p.state=ProcessState::TERMINATED;
                done[idx]=true; completed++; prev=-1;
            }
        }
    }
    res.processes=procs; res.total_context_switches=ctx;
    {
        double tw=0,tt=0,tr=0;int busy=0;
        for(auto& e:res.gantt)if(e.pid!=-1)busy+=(e.end-e.start);
        int mk=res.gantt.empty()?1:res.gantt.back().end;
        for(auto& p2:res.processes){tw+=p2.waiting_time;tt+=p2.turnaround_time;tr+=p2.response_time;}
        int sz=(int)res.processes.size();
        res.avg_waiting_time=sz?tw/sz:0;res.avg_turnaround_time=sz?tt/sz:0;
        res.avg_response_time=sz?tr/sz:0;res.cpu_utilization=mk?(double)busy/mk*100:0;
        res.throughput=mk?(double)sz/mk:0;
    }
    return res;
}

// ─────────────────────────────────────────────────────────────
//  MLFQ — Multi-Level Feedback Queue
// ─────────────────────────────────────────────────────────────
SchedulerResult MLFQ::run(std::vector<PCB> procs) {
    SchedulerResult res;
    int n = (int)procs.size();
    int levels = cfg.num_queues;

    // One ready queue per level
    std::vector<std::queue<int>> queues(levels);
    std::vector<bool> arrived(n, false);
    std::vector<bool> done(n, false);

    int time=0, completed=0, ctx=0;

    auto enqueueArrivals = [&](){
        for(int i=0;i<n;i++){
            if(!arrived[i]&&!done[i]&&procs[i].arrival_time<=time){
                queues[procs[i].queue_level].push(i);
                arrived[i]=true;
            }
        }
    };

    int last_boost = 0;

    while(completed<n){
        // Periodic boost (starvation prevention)
        if(time-last_boost>=cfg.boost_interval){
            for(int l=1;l<levels;l++){
                while(!queues[l].empty()){
                    int idx=queues[l].front(); queues[l].pop();
                    procs[idx].queue_level=0;
                    queues[0].push(idx);
                }
            }
            last_boost=time;
        }

        enqueueArrivals();

        // Find highest-priority non-empty queue
        int level=-1;
        for(int l=0;l<levels;l++){
            if(!queues[l].empty()){level=l;break;}
        }

        if(level==-1){
            // Advance to next arrival
            int next=INT_MAX;
            for(int i=0;i<n;i++) if(!done[i]&&!arrived[i]) next=std::min(next,procs[i].arrival_time);
            if(next==INT_MAX) break;
            res.gantt.push_back({-1,"IDLE",time,next});
            time=next; enqueueArrivals(); continue;
        }

        int i=queues[level].front(); queues[level].pop();
        auto& p=procs[i];
        if(p.start_time==-1){p.start_time=time;p.response_time=time-p.arrival_time;}

        int q=cfg.quantums[level];
        int run_for=std::min(q,p.remaining_time);
        res.gantt.push_back({p.pid,p.name,time,time+run_for});
        p.remaining_time-=run_for; time+=run_for; ctx++;

        enqueueArrivals();

        if(p.remaining_time==0){
            p.finish_time=time;
            p.turnaround_time=p.finish_time-p.arrival_time;
            p.waiting_time=p.turnaround_time-p.burst_time;
            p.state=ProcessState::TERMINATED;
            done[i]=true; completed++;
        } else {
            // Demote to lower queue
            int next_level=std::min(level+1,levels-1);
            p.queue_level=next_level;
            queues[next_level].push(i);
        }
    }

    res.processes=procs; res.total_context_switches=ctx;
    {
        double tw=0,tt=0,tr=0;int busy=0;
        for(auto& e:res.gantt)if(e.pid!=-1)busy+=(e.end-e.start);
        int mk=res.gantt.empty()?1:res.gantt.back().end;
        for(auto& p2:res.processes){tw+=p2.waiting_time;tt+=p2.turnaround_time;tr+=p2.response_time;}
        int sz=(int)res.processes.size();
        res.avg_waiting_time=sz?tw/sz:0;res.avg_turnaround_time=sz?tt/sz:0;
        res.avg_response_time=sz?tr/sz:0;res.cpu_utilization=mk?(double)busy/mk*100:0;
        res.throughput=mk?(double)sz/mk:0;
    }
    return res;
}
