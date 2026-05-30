#pragma once
#include <vector>
#include <unordered_map>
#include <list>
#include <string>
#include <deque>

// ─────────────────────────────────────────────────────────────
//  Page Replacement Algorithms
// ─────────────────────────────────────────────────────────────
enum class PageAlgo { LRU, FIFO, OPTIMAL };

struct PageResult {
    int  page_faults;
    int  page_hits;
    double hit_ratio;
    std::vector<std::pair<int,bool>> trace; // (page, was_fault)
};

// ─────────────────────────────────────────────────────────────
//  LRU Page Replacement
// ─────────────────────────────────────────────────────────────
class LRUCache {
    int capacity;
    std::list<int> lru_list;
    std::unordered_map<int, std::list<int>::iterator> page_map;
public:
    explicit LRUCache(int cap) : capacity(cap) {}
    bool access(int page);   // returns true on hit
    std::vector<int> frames() const;
};

// ─────────────────────────────────────────────────────────────
//  FIFO Page Replacement
// ─────────────────────────────────────────────────────────────
class FIFOCache {
    int capacity;
    std::deque<int> queue;
    std::unordered_map<int,bool> in_mem;
public:
    explicit FIFOCache(int cap) : capacity(cap) {}
    bool access(int page);
};

// ─────────────────────────────────────────────────────────────
//  Optimal Page Replacement
// ─────────────────────────────────────────────────────────────
class OptimalCache {
    int capacity;
    std::vector<int> frames;
public:
    explicit OptimalCache(int cap) : capacity(cap) {}
    // needs full reference string to look ahead
    PageResult simulate(const std::vector<int>& refs);
};

// ─────────────────────────────────────────────────────────────
//  TLB Simulation
// ─────────────────────────────────────────────────────────────
struct TLBEntry {
    int vpn;   // virtual page number
    int pfn;   // physical frame number
    bool valid;
};

class TLB {
    int size;
    std::list<TLBEntry> entries;   // front = MRU
public:
    explicit TLB(int sz = 8) : size(sz) {}
    // returns pfn if hit, -1 on miss
    int lookup(int vpn);
    void insert(int vpn, int pfn);
    void flush();
    int hits = 0, misses = 0;
};

// ─────────────────────────────────────────────────────────────
//  Virtual Memory Manager
// ─────────────────────────────────────────────────────────────
class VirtualMemoryManager {
    int num_frames;
    TLB tlb;
    std::unordered_map<int,int> page_table; // vpn -> pfn
    std::vector<int> frame_pool;
    int next_frame = 0;

    LRUCache lru;
public:
    explicit VirtualMemoryManager(int frames = 16)
        : num_frames(frames), tlb(8), lru(frames) {}

    // Translate virtual address; returns physical address or -1 on fault
    int translate(int virtual_addr, int page_size = 256);

    PageResult runSimulation(const std::vector<int>& ref_string,
                             int frames, PageAlgo algo);

    double tlbHitRatio() const {
        int total = tlb.hits + tlb.misses;
        return total ? (double)tlb.hits / total : 0.0;
    }
};
