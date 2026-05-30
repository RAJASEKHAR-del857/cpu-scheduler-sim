#include "memory.h"
#include <algorithm>
#include <climits>

// ─────────────────────────────────────────────────────────────
//  LRU Cache
// ─────────────────────────────────────────────────────────────
bool LRUCache::access(int page) {
    auto it = page_map.find(page);
    if (it != page_map.end()) {
        // Hit: move to front (most recently used)
        lru_list.erase(it->second);
        lru_list.push_front(page);
        it->second = lru_list.begin();
        return true;
    }
    // Miss: evict LRU if full
    if ((int)lru_list.size() >= capacity) {
        int evict = lru_list.back();
        lru_list.pop_back();
        page_map.erase(evict);
    }
    lru_list.push_front(page);
    page_map[page] = lru_list.begin();
    return false;
}

std::vector<int> LRUCache::frames() const {
    return std::vector<int>(lru_list.begin(), lru_list.end());
}

// ─────────────────────────────────────────────────────────────
//  FIFO Cache
// ─────────────────────────────────────────────────────────────
bool FIFOCache::access(int page) {
    if (in_mem.count(page)) return true;
    if ((int)queue.size() >= capacity) {
        int evict = queue.front(); queue.pop_front();
        in_mem.erase(evict);
    }
    queue.push_back(page);
    in_mem[page] = true;
    return false;
}

// ─────────────────────────────────────────────────────────────
//  Optimal Cache
// ─────────────────────────────────────────────────────────────
PageResult OptimalCache::simulate(const std::vector<int>& refs) {
    PageResult pr;
    frames.clear();

    for (int i = 0; i < (int)refs.size(); i++) {
        int page = refs[i];
        // Check if already in frames
        auto fit = std::find(frames.begin(), frames.end(), page);
        if (fit != frames.end()) {
            pr.page_hits++;
            pr.trace.push_back({page, false});
            continue;
        }
        pr.page_faults++;
        pr.trace.push_back({page, true});

        if ((int)frames.size() < capacity) {
            frames.push_back(page);
        } else {
            // Find the frame whose next use is farthest in the future
            int evict_idx = -1, farthest = -1;
            for (int f = 0; f < (int)frames.size(); f++) {
                int next_use = INT_MAX;
                for (int j = i+1; j < (int)refs.size(); j++) {
                    if (refs[j] == frames[f]) { next_use = j; break; }
                }
                if (next_use > farthest) { farthest = next_use; evict_idx = f; }
            }
            frames[evict_idx] = page;
        }
    }
    int total = pr.page_hits + pr.page_faults;
    pr.hit_ratio = total ? (double)pr.page_hits / total : 0.0;
    return pr;
}

// ─────────────────────────────────────────────────────────────
//  TLB
// ─────────────────────────────────────────────────────────────
int TLB::lookup(int vpn) {
    for (auto& e : entries) {
        if (e.valid && e.vpn == vpn) { hits++; return e.pfn; }
    }
    misses++;
    return -1;
}

void TLB::insert(int vpn, int pfn) {
    // Evict LRU if full
    if ((int)entries.size() >= size) entries.pop_back();
    entries.push_front({vpn, pfn, true});
}

void TLB::flush() { entries.clear(); }

// ─────────────────────────────────────────────────────────────
//  Virtual Memory Manager
// ─────────────────────────────────────────────────────────────
int VirtualMemoryManager::translate(int virtual_addr, int page_size) {
    int vpn   = virtual_addr / page_size;
    int offset= virtual_addr % page_size;

    // 1. TLB lookup
    int pfn = tlb.lookup(vpn);
    if (pfn != -1) return pfn * page_size + offset;

    // 2. Page table lookup
    auto it = page_table.find(vpn);
    if (it != page_table.end()) {
        pfn = it->second;
        tlb.insert(vpn, pfn);
        return pfn * page_size + offset;
    }

    // 3. Page fault — allocate frame via LRU
    lru.access(vpn);
    pfn = next_frame % num_frames;
    next_frame++;
    page_table[vpn] = pfn;
    tlb.insert(vpn, pfn);
    return pfn * page_size + offset;
}

PageResult VirtualMemoryManager::runSimulation(
        const std::vector<int>& ref_string, int frames, PageAlgo algo) {
    PageResult pr;

    if (algo == PageAlgo::OPTIMAL) {
        OptimalCache opt(frames);
        return opt.simulate(ref_string);
    } else if (algo == PageAlgo::FIFO) {
        FIFOCache fifo(frames);
        for (int p : ref_string) {
            bool hit = fifo.access(p);
            pr.trace.push_back({p, !hit});
            if (hit) pr.page_hits++; else pr.page_faults++;
        }
    } else { // LRU
        LRUCache lru_c(frames);
        for (int p : ref_string) {
            bool hit = lru_c.access(p);
            pr.trace.push_back({p, !hit});
            if (hit) pr.page_hits++; else pr.page_faults++;
        }
    }
    int total = pr.page_hits + pr.page_faults;
    pr.hit_ratio = total ? (double)pr.page_hits / total : 0.0;
    return pr;
}
