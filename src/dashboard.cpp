#include "dashboard.h"
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstdio>

// Color pair IDs
#define C_TITLE   1
#define C_HEADER  2
#define C_PROC    3
#define C_IDLE    4
#define C_STAT    5
#define C_BORDER  6
#define C_GANTT1  7
#define C_GANTT2  8

Dashboard::Dashboard() { getmaxyx(stdscr, rows, cols); }
Dashboard::~Dashboard() { cleanup(); }

void Dashboard::init() {
    initscr();
    cbreak(); noecho(); keypad(stdscr, TRUE);
    curs_set(0);
    start_color();

    init_pair(C_TITLE,  COLOR_BLACK,  COLOR_CYAN);
    init_pair(C_HEADER, COLOR_YELLOW, COLOR_BLACK);
    init_pair(C_PROC,   COLOR_GREEN,  COLOR_BLACK);
    init_pair(C_IDLE,   COLOR_RED,    COLOR_BLACK);
    init_pair(C_STAT,   COLOR_CYAN,   COLOR_BLACK);
    init_pair(C_BORDER, COLOR_BLUE,   COLOR_BLACK);
    init_pair(C_GANTT1, COLOR_BLACK,  COLOR_GREEN);
    init_pair(C_GANTT2, COLOR_BLACK,  COLOR_YELLOW);

    getmaxyx(stdscr, rows, cols);
}

void Dashboard::cleanup() {
    if (isendwin()) return;
    endwin();
}

void Dashboard::drawBorder(WINDOW* w, const std::string& title, int cp) {
    wattron(w, COLOR_PAIR(cp));
    box(w, 0, 0);
    int ww = getmaxx(w);
    mvwprintw(w, 0, (ww - (int)title.size() - 2) / 2,
              " %s ", title.c_str());
    wattroff(w, COLOR_PAIR(cp));
}

void Dashboard::renderGantt(const std::vector<GanttEntry>& gantt, WINDOW* w) {
    if (!w) return;
    drawBorder(w, "Gantt Chart", C_BORDER);
    int ww = getmaxx(w) - 2;

    // Find total time
    int total = gantt.empty() ? 1 : gantt.back().end;
    double scale = (double)(ww - 2) / total;

    int row = 1;
    // Draw process bar
    wmove(w, row, 1);
    for (const auto& e : gantt) {
        int len = std::max(1, (int)((e.end - e.start) * scale));
        if (e.pid == -1) {
            wattron(w, COLOR_PAIR(C_IDLE));
            for (int i = 0; i < len && getcurx(w) < ww; i++) waddch(w, ' ');
            wattroff(w, COLOR_PAIR(C_IDLE));
        } else {
            int cp = (e.pid % 2 == 0) ? C_GANTT1 : C_GANTT2;
            wattron(w, COLOR_PAIR(cp));
            // Center the name
            std::string lbl = e.name.size() <= (size_t)len ?
                              e.name.substr(0, len) : std::string(len, '#');
            for (char c : lbl)
                if (getcurx(w) < ww) waddch(w, c);
            wattroff(w, COLOR_PAIR(cp));
        }
    }

    // Time axis
    row = 2;
    wmove(w, row, 1);
    for (const auto& e : gantt) {
        int x = 1 + (int)(e.start * scale);
        mvwprintw(w, row, x, "|%-3d", e.start);
    }
    if (!gantt.empty()) {
        int x = 1 + (int)(gantt.back().end * scale);
        mvwprintw(w, row, std::min(x, ww-1), "%d", gantt.back().end);
    }
}

void Dashboard::renderStats(const SchedulerResult& res, WINDOW* w) {
    if (!w) return;
    drawBorder(w, "Statistics", C_BORDER);

    auto pr = [&](int r, const char* lbl, double val, const char* unit) {
        wattron(w, COLOR_PAIR(C_STAT));
        mvwprintw(w, r, 2, "%-28s", lbl);
        wattroff(w, COLOR_PAIR(C_STAT));
        mvwprintw(w, r, 30, "%8.2f %s", val, unit);
    };

    pr(1, "Avg Waiting Time",        res.avg_waiting_time,    "units");
    pr(2, "Avg Turnaround Time",     res.avg_turnaround_time, "units");
    pr(3, "Avg Response Time",       res.avg_response_time,   "units");
    pr(4, "CPU Utilization",         res.cpu_utilization,     "%");
    pr(5, "Throughput",              res.throughput,          "proc/unit");
    pr(6, "Total Context Switches",  (double)res.total_context_switches, "");
}

void Dashboard::renderProcessTable(const std::vector<PCB>& procs, WINDOW* w) {
    if (!w) return;
    drawBorder(w, "Process Table", C_BORDER);

    wattron(w, COLOR_PAIR(C_HEADER));
    mvwprintw(w, 1, 2, "%-6s %-8s %-8s %-8s %-8s %-10s %-12s %-10s",
              "PID","Name","Arrival","Burst","Priority","Start","Turnaround","Wait");
    wattroff(w, COLOR_PAIR(C_HEADER));

    int row = 2;
    for (const auto& p : procs) {
        int cp = (p.pid % 2 == 0) ? C_PROC : C_STAT;
        wattron(w, COLOR_PAIR(cp));
        mvwprintw(w, row++, 2,
                  "%-6d %-8s %-8d %-8d %-8d %-10d %-12d %-10d",
                  p.pid, p.name.c_str(), p.arrival_time, p.burst_time,
                  p.priority, p.start_time, p.turnaround_time, p.waiting_time);
        wattroff(w, COLOR_PAIR(cp));
        if (row >= getmaxy(w) - 1) break;
    }
}

void Dashboard::renderSchedulerResult(const SchedulerResult& res,
                                       const std::string& algo_name) {
    clear();
    getmaxyx(stdscr, rows, cols);

    // Title bar
    attron(COLOR_PAIR(C_TITLE) | A_BOLD);
    mvprintw(0, 0, "%*s", cols, "");
    mvprintw(0, (cols - (int)algo_name.size() - 14) / 2,
             " CPU Scheduler: %s ", algo_name.c_str());
    attroff(COLOR_PAIR(C_TITLE) | A_BOLD);

    int gantt_h  = 5;
    int stats_h  = 10;
    int proc_h   = rows - gantt_h - stats_h - 2;

    gantt_win = newwin(gantt_h,  cols,     1,        0);
    stats_win = newwin(stats_h,  cols / 2, gantt_h + 1, 0);
    proc_win  = newwin(proc_h,   cols,     gantt_h + stats_h + 1, 0);

    renderGantt(res.gantt, gantt_win);
    renderStats(res, stats_win);
    renderProcessTable(res.processes, proc_win);

    wrefresh(gantt_win);
    wrefresh(stats_win);
    wrefresh(proc_win);
    refresh();

    waitForKey();

    delwin(gantt_win); delwin(stats_win); delwin(proc_win);
    gantt_win = stats_win = proc_win = nullptr;
}

void Dashboard::renderMemoryResult(const PageResult& res,
                                    const std::string& algo_name, int frames) {
    clear();
    getmaxyx(stdscr, rows, cols);

    attron(COLOR_PAIR(C_TITLE) | A_BOLD);
    mvprintw(0, 0, "%*s", cols, "");
    mvprintw(0, (cols - (int)algo_name.size() - 20) / 2,
             " Page Replacement: %s (frames=%d) ", algo_name.c_str(), frames);
    attroff(COLOR_PAIR(C_TITLE) | A_BOLD);

    WINDOW* w = newwin(rows - 2, cols, 1, 0);
    drawBorder(w, "Page Reference Trace", C_BORDER);

    wattron(w, COLOR_PAIR(C_HEADER));
    mvwprintw(w, 1, 2, "%-8s %-8s", "Page", "Result");
    wattroff(w, COLOR_PAIR(C_HEADER));

    int row = 2, col_off = 2;
    for (const auto& [pg, fault] : res.trace) {
        int cp = fault ? C_IDLE : C_PROC;
        wattron(w, COLOR_PAIR(cp));
        mvwprintw(w, row, col_off, "%-8d %-8s", pg, fault ? "FAULT" : "HIT");
        wattroff(w, COLOR_PAIR(cp));
        row++;
        if (row >= getmaxy(w) - 3) { row = 2; col_off += 20; }
        if (col_off >= cols - 22) break;
    }

    wattron(w, COLOR_PAIR(C_STAT));
    mvwprintw(w, getmaxy(w)-2, 2,
              "Faults: %d  Hits: %d  Hit Ratio: %.2f%%",
              res.page_faults, res.page_hits, res.hit_ratio * 100.0);
    wattroff(w, COLOR_PAIR(C_STAT));

    wrefresh(w); refresh();
    waitForKey();
    delwin(w);
}

void Dashboard::renderIPCLog(const std::vector<std::string>& log,
                              const std::string& title) {
    clear();
    getmaxyx(stdscr, rows, cols);
    attron(COLOR_PAIR(C_TITLE) | A_BOLD);
    mvprintw(0, 0, "%*s", cols, "");
    mvprintw(0, (cols - (int)title.size() - 4) / 2, "  %s  ", title.c_str());
    attroff(COLOR_PAIR(C_TITLE) | A_BOLD);

    WINDOW* w = newwin(rows - 2, cols, 1, 0);
    drawBorder(w, title, C_BORDER);

    int row = 1;
    for (const auto& line : log) {
        if (row >= getmaxy(w) - 1) break;
        mvwprintw(w, row++, 2, "%s", line.substr(0, cols - 4).c_str());
    }
    wrefresh(w); refresh();
    waitForKey();
    delwin(w);
}

void Dashboard::waitForKey(const std::string& prompt) {
    int r, c; getmaxyx(stdscr, r, c);
    attron(COLOR_PAIR(C_TITLE));
    mvprintw(r-1, (c - (int)prompt.size()) / 2, "%s", prompt.c_str());
    attroff(COLOR_PAIR(C_TITLE));
    refresh();
    getch();
}

// ─────────────────────────────────────────────────────────────
//  Plain terminal (non-ncurses) fallback
// ─────────────────────────────────────────────────────────────
void Dashboard::printSchedulerResult(const SchedulerResult& res,
                                      const std::string& algo_name) {
    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║  Algorithm: %-32s║\n", algo_name.c_str());
    printf("╚══════════════════════════════════════════════╝\n");

    printf("\n[ Gantt Chart ]\n  ");
    for (const auto& e : res.gantt) {
        if (e.pid == -1) printf("[IDLE:%d-%d]", e.start, e.end);
        else             printf("[%s:%d-%d]",   e.name.c_str(), e.start, e.end);
    }
    printf("\n");

    printf("\n%-6s %-8s %-8s %-8s %-8s %-12s %-10s\n",
           "PID","Name","Arrival","Burst","Start","Turnaround","Wait");
    for (const auto& p : res.processes) {
        printf("%-6d %-8s %-8d %-8d %-8d %-12d %-10d\n",
               p.pid, p.name.c_str(), p.arrival_time, p.burst_time,
               p.start_time, p.turnaround_time, p.waiting_time);
    }

    printf("\n  Avg Waiting:       %.2f\n", res.avg_waiting_time);
    printf("  Avg Turnaround:    %.2f\n", res.avg_turnaround_time);
    printf("  Avg Response:      %.2f\n", res.avg_response_time);
    printf("  CPU Utilization:   %.2f%%\n", res.cpu_utilization);
    printf("  Throughput:        %.4f proc/unit\n", res.throughput);
    printf("  Context Switches:  %d\n\n", res.total_context_switches);
}

void Dashboard::printMemoryResult(const PageResult& res,
                                   const std::string& algo_name, int frames) {
    printf("\n[ %s | frames=%d ] Faults=%d Hits=%d HitRatio=%.2f%%\n",
           algo_name.c_str(), frames, res.page_faults,
           res.page_hits, res.hit_ratio * 100.0);
}
