# CPU Scheduler & Process Lifecycle Simulator

A comprehensive OS concepts simulator built in **C++17** using **POSIX** APIs on Linux.

---

## Features

### Scheduling Algorithms
| Algorithm | Type | Starvation Prevention |
|-----------|------|-----------------------|
| FCFS | Non-preemptive | N/A |
| SJF | Non-preemptive | — |
| SRTF | Preemptive | — |
| Round Robin | Preemptive | Implicit (cyclic) |
| Priority | Both modes | Aging (priority boost) |
| MLFQ | Preemptive | Priority boost timer |

### Virtual Memory Subsystem
- **LRU** (Least Recently Used)
- **FIFO** (First-In First-Out)
- **Optimal** (Bélády's algorithm)
- **TLB simulation** with configurable size

### IPC Mechanisms
- **Shared memory** ring buffer (Producer-Consumer)
- **Named pipes** (unidirectional)
- **POSIX Message Queues** (multi-sender/receiver)
- **Reader-Writer** (semaphore-based, readers-priority)

### ncurses Dashboard
- Interactive menu (keyboard navigation)
- Gantt chart visualisation
- Per-process statistics table
- Aggregate metrics: avg wait, turnaround, response, CPU utilisation, throughput

---

## Build

### Prerequisites
```bash
sudo apt-get install g++ libncurses5-dev libncursesw5-dev
```

### Compile
```bash
make
```

### Run
```bash
make run
# or directly:
./cpu_scheduler_sim
```

### Clean
```bash
make clean
```

---

## Project Structure

```
cpu_scheduler_sim/
├── include/
│   ├── process.h       # PCB, GanttEntry, SchedulerResult
│   ├── scheduler.h     # All scheduler declarations
│   ├── memory.h        # Page replacement & TLB
│   ├── ipc.h           # IPC mechanism declarations
│   └── dashboard.h     # ncurses UI
├── src/
│   ├── main.cpp              # Entry point + menu
│   ├── scheduler_basic.cpp   # FCFS, SJF, SRTF
│   ├── scheduler_advanced.cpp# Round Robin, Priority, MLFQ
│   ├── memory.cpp            # Virtual memory impl
│   ├── ipc.cpp               # POSIX threads/semaphores IPC
│   └── dashboard.cpp         # ncurses rendering
├── Makefile
└── README.md
```

---

## Interview Discussion Points

### Scheduling

**Q: How does starvation prevention work in Priority scheduling?**
> Aging: every `aging_threshold` ticks a waiting process gets its priority value decremented (higher priority). This ensures even low-priority processes eventually run.

**Q: How does MLFQ prevent starvation?**
> A periodic `boost_interval` moves all processes from lower queues back to queue 0, giving them a chance to run at full quantum.

**Q: What is the context-switch model?**
> Every time the running process changes (`idx != prev` in preemptive schedulers), `total_context_switches` increments. This is tracked in the `SchedulerResult`.

### Memory

**Q: Why does Optimal have fewer faults than LRU or FIFO?**
> Optimal has perfect future knowledge and evicts the page whose next use is farthest away — it sets the theoretical lower bound on page faults.

**Q: What is Bélády's anomaly?**
> For FIFO, adding more frames can sometimes *increase* page faults. LRU and Optimal are stack algorithms and don't exhibit this.

**Q: How does TLB work here?**
> On each `translate()` call: check TLB (O(size)) → on miss, check page table → on page fault, allocate a physical frame via LRU eviction and update both page table and TLB.

### IPC

**Q: How does Producer-Consumer avoid race conditions?**
> Three POSIX semaphores: `mutex` (1) guards the shared counter; `empty_slots` (N) blocks producers when buffer is full; `full_slots` (0) blocks consumers when buffer is empty.

**Q: What is the critical section in Reader-Writer?**
> The `reader_count` variable. It's protected by `mutex`. The first reader to arrive acquires `write_lock`; the last reader to leave releases it. Writers always hold `write_lock` exclusively.

---

## Sample Output (non-ncurses fallback)

```
╔══════════════════════════════════════════════╗
║  Algorithm: FCFS                             ║
╚══════════════════════════════════════════════╝

[ Gantt Chart ]
  [P1:0-6][P2:6-10][P3:10-18][P4:18-21][P5:21-26][P6:26-28]

PID    Name     Arrival  Burst    Start    Turnaround Wait
1      P1       0        6        0        6          0
2      P2       1        4        6        9          5
...

  Avg Waiting:       8.50
  Avg Turnaround:    12.83
  CPU Utilization:   100.00%
  Context Switches:  6
```
