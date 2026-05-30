#pragma once
#include "scheduler.h"
#include "memory.h"
#include "ipc.h"
#include <ncurses.h>

// ─────────────────────────────────────────────────────────────
//  ncurses Dashboard
// ─────────────────────────────────────────────────────────────
class Dashboard {
    WINDOW* main_win  = nullptr;
    WINDOW* gantt_win = nullptr;
    WINDOW* stats_win = nullptr;
    WINDOW* proc_win  = nullptr;
    int rows, cols;

    void drawBorder(WINDOW* w, const std::string& title, int color_pair);
public:
    Dashboard();
    ~Dashboard();

    void init();
    void renderSchedulerResult(const SchedulerResult& res,
                               const std::string& algo_name);
    void renderGantt(const std::vector<GanttEntry>& gantt, WINDOW* w);
    void renderStats(const SchedulerResult& res, WINDOW* w);
    void renderProcessTable(const std::vector<PCB>& procs, WINDOW* w);
    void renderMemoryResult(const PageResult& res,
                            const std::string& algo_name, int frames);
    void renderIPCLog(const std::vector<std::string>& log,
                      const std::string& title);
    void waitForKey(const std::string& prompt = "Press any key to continue...");
    void cleanup();

    // non-interactive (plain terminal) fallback
    static void printSchedulerResult(const SchedulerResult& res,
                                     const std::string& algo_name);
    static void printMemoryResult(const PageResult& res,
                                  const std::string& algo_name, int frames);
};
