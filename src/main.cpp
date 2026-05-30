#include "scheduler.h"
#include "memory.h"
#include "ipc.h"
#include "dashboard.h"

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <ncurses.h>

// ─────────────────────────────────────────────────────────────
//  Sample workloads
// ─────────────────────────────────────────────────────────────
static std::vector<PCB> sampleProcesses() {
    return {
        PCB(1, "P1", 0,  6, 3),
        PCB(2, "P2", 1,  4, 1),
        PCB(3, "P3", 2,  8, 4),
        PCB(4, "P4", 3,  3, 2),
        PCB(5, "P5", 4,  5, 2),
        PCB(6, "P6", 5,  2, 5),
    };
}

static std::vector<int> samplePageRefs() {
    return {7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1};
}

// ─────────────────────────────────────────────────────────────
//  ncurses main menu
// ─────────────────────────────────────────────────────────────
static int showMenu(const std::vector<std::string>& items, const std::string& title) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int h = (int)items.size() + 4;
    int w = 50;
    int y = (rows - h) / 2, x = (cols - w) / 2;
    WINDOW* win = newwin(h, w, y, x);

    init_pair(10, COLOR_BLACK, COLOR_CYAN);
    init_pair(11, COLOR_CYAN,  COLOR_BLACK);
    init_pair(12, COLOR_WHITE, COLOR_BLUE);

    int sel = 0;
    int ch;
    while (true) {
        werase(win);
        wattron(win, COLOR_PAIR(10) | A_BOLD);
        box(win, 0, 0);
        mvwprintw(win, 0, (w - (int)title.size() - 2)/2, " %s ", title.c_str());
        wattroff(win, COLOR_PAIR(10) | A_BOLD);

        for (int i = 0; i < (int)items.size(); i++) {
            if (i == sel) {
                wattron(win, COLOR_PAIR(12) | A_BOLD);
                mvwprintw(win, i+2, 2, " %-44s", items[i].c_str());
                wattroff(win, COLOR_PAIR(12) | A_BOLD);
            } else {
                wattron(win, COLOR_PAIR(11));
                mvwprintw(win, i+2, 2, " %-44s", items[i].c_str());
                wattroff(win, COLOR_PAIR(11));
            }
        }
        wrefresh(win);
        ch = getch();
        if (ch == KEY_UP)    sel = (sel + (int)items.size() - 1) % (int)items.size();
        if (ch == KEY_DOWN)  sel = (sel + 1) % (int)items.size();
        if (ch == '\n' || ch == KEY_ENTER || ch == ' ') break;
        if (ch == 'q' || ch == 'Q') { sel = (int)items.size()-1; break; }
    }
    delwin(win);
    return sel;
}

// ─────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────
int main() {
    Dashboard dash;
    dash.init();

    bool running = true;
    while (running) {
        std::vector<std::string> menu = {
            "1. Run All Scheduling Algorithms",
            "2. FCFS",
            "3. SJF (Non-Preemptive)",
            "4. SRTF (Preemptive SJF)",
            "5. Round Robin (Q=2)",
            "6. Priority (Non-Preemptive)",
            "7. Priority (Preemptive)",
            "8. MLFQ",
            "9. Virtual Memory Simulation",
            "10. IPC: Producer-Consumer",
            "11. IPC: Reader-Writer",
            "12. IPC: Message Queue Demo",
            "13. IPC: Pipe Demo",
            "14. Quit"
        };

        int choice = showMenu(menu, "OS Scheduler Simulator");
        auto procs = sampleProcesses();
        auto refs  = samplePageRefs();
        VirtualMemoryManager vmm;

        auto runSched = [&](Scheduler* s) {
            auto res = s->run(procs);
            dash.renderSchedulerResult(res, s->name());
        };

        switch (choice) {
            case 0: {
                std::vector<std::unique_ptr<Scheduler>> algos;
                algos.emplace_back(new FCFS());
                algos.emplace_back(new SJF());
                algos.emplace_back(new SRTF());
                algos.emplace_back(new RoundRobin(2));
                algos.emplace_back(new PriorityScheduler(false));
                algos.emplace_back(new PriorityScheduler(true));
                algos.emplace_back(new MLFQ());
                for (auto& a : algos) runSched(a.get());
                break;
            }
            case 1:  { FCFS f;               runSched(&f); break; }
            case 2:  { SJF  s;               runSched(&s); break; }
            case 3:  { SRTF sr;              runSched(&sr); break; }
            case 4:  { RoundRobin rr(2);     runSched(&rr); break; }
            case 5:  { PriorityScheduler p;  runSched(&p); break; }
            case 6:  { PriorityScheduler pp(true); runSched(&pp); break; }
            case 7:  { MLFQ m;               runSched(&m); break; }
            case 8: {
                std::vector<std::pair<PageAlgo,std::string>> algos = {
                    {PageAlgo::LRU, "LRU"},
                    {PageAlgo::FIFO,"FIFO"},
                    {PageAlgo::OPTIMAL,"Optimal"}
                };
                for (auto& [algo, aname] : algos) {
                    auto pr = vmm.runSimulation(refs, 3, algo);
                    dash.renderMemoryResult(pr, aname, 3);
                }
                break;
            }
            case 9: {
                ProducerConsumerConfig cfg{2,2,4};
                auto events = runProducerConsumer(cfg);
                std::vector<std::string> log;
                for (auto& e : events)
                    log.push_back(e.actor + " " + e.action +
                                  " val=" + std::to_string(e.value) +
                                  " buf=" + std::to_string(e.buffer_fill));
                dash.renderIPCLog(log, "Producer-Consumer");
                break;
            }
            case 10: {
                RWConfig rw{4,2,3};
                auto events = runReaderWriter(rw);
                std::vector<std::string> log;
                for (auto& e : events)
                    log.push_back(e.actor + " " + e.action +
                                  " val=" + std::to_string(e.shared_value));
                dash.renderIPCLog(log, "Reader-Writer");
                break;
            }
            case 11: {
                auto events = runMessageQueueDemo(2,2,3);
                std::vector<std::string> log;
                for (auto& e : events)
                    log.push_back(e.sender + " -> " + e.receiver +
                                  " : " + e.message +
                                  " [prio=" + std::to_string(e.priority) + "]");
                dash.renderIPCLog(log, "Message Queue");
                break;
            }
            case 12: {
                auto res = runPipeDemo("Hello from CPU Scheduler Simulator!");
                dash.renderIPCLog(res.log, "Pipe IPC Demo");
                break;
            }
            case 13:
            default:
                running = false;
                break;
        }
    }

    dash.cleanup();
    return 0;
}
