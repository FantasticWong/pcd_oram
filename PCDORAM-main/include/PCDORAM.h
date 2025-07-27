#pragma once

#include <iostream>
#include <cmath>
#include <list>
#include <cassert>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <queue>
#include <algorithm>
#include <chrono>
#include <random>
#include <unordered_map>
#include <set>
#include "LocalCacheLine.h"

using namespace std;

class Stash5 {
private:
    int max_stash_size;		
    int peak_occupancy;		
    int last_occupancy;		
    int Z_value;
    int L;
    int block_size;



    friend class PCDORAM;
public:

    unordered_map<int64_t, LocalCacheLine> temporal_area;
    unordered_map<int64_t, list<LocalCacheLine>::iterator> candidate_area_key;
    unordered_map<int64_t, int64_t> candidate_area_key_freq;
    unordered_map<int64_t, list<LocalCacheLine> > candidate_area_freq;

    Stash5() {
        max_stash_size = 1024 * 1024;		// in MB
        peak_occupancy = 0;
        last_occupancy = 0;
    }
    void setMaxStashSize(int size) { max_stash_size = size; } 
    void setZvalue(int Z) { Z_value = Z; }
    void setL(int l) { L = l; }
    void setBlocksize(int bs) { block_size = bs; }
    int getMaxStashSize() { return max_stash_size; }
    int getPeakOccupancy() { return peak_occupancy; }
    int getLastOccupancy() { return last_occupancy; }
    int getCurrentStashSize() { return temporal_area.size() + candidate_area_key.size(); }
    void updatePeakAndLastOccupancy() {
        last_occupancy = temporal_area.size() + candidate_area_key.size();
        peak_occupancy = (last_occupancy > peak_occupancy) ? last_occupancy : peak_occupancy;
    }

    bool isFull(int margin = 0) {
        int upperLimit = max_stash_size - margin - Z_value * L;
        if (getCurrentStashSize() >= upperLimit)
            return true;
        return false;
    }

    bool isAlmostFull() {
        int upperLimit = max_stash_size - Z_value * L;
        if (getCurrentStashSize() >= 0.9 * upperLimit)
            return true;
        return false;
    }

    bool isTemporalAreaAlmostEmpty() {
        int downerLimit = Z_value * L;
        //	int downerLimit = L;
        if (temporal_area.size() <= downerLimit)
            return true;
        return false;
    }

    void displayStash() {
        for (auto& ele : candidate_area_freq) {
            for (auto& node : ele.second) {
                cout << "(" << ele.first << ", " << node.id << ", " << *(node.position) << "), ";
            }
        }
        cout << endl;

        for (auto& t_ele : temporal_area)
            cout << "(" << t_ele.second.id << ", " << *(t_ele.second.position) << "), ";
        cout << endl;
    }

    LocalCacheLine getFromCandidateArea(LocalCacheLine data) {

    }

    void putIntoCandidateArea(LocalCacheLine data) {
        auto key_it = candidate_area_key.find(data.id);
        if (key_it != candidate_area_key.end()) { 
            list<LocalCacheLine>::iterator no = candidate_area_key[data.id];
            int freq = candidate_area_key_freq[data.id]; 
            candidate_area_freq[freq].erase(no);
            if (candidate_area_freq[freq].size() == 0) {
                candidate_area_freq.erase(freq);
            }
            candidate_area_key_freq[data.id] = freq + 1; 
            candidate_area_freq[freq + 1].push_front(data);
            candidate_area_key[data.id] = candidate_area_freq[freq + 1].begin();
        }
        else {
            candidate_area_key_freq[data.id] = 1;
            candidate_area_freq[1].push_front(data);
            candidate_area_key[data.id] = candidate_area_freq[1].begin();
        }
    }

    bool getFromTemporalArea(int64_t block_id) {
        if (temporal_area.find(block_id) != temporal_area.end()) {
            return true;
        }
        return false;
    }

    void putIntoTemporalArea(LocalCacheLine data) {
        temporal_area[data.id] = data;
    }

    ~Stash5() { }
};



class PCDORAM {
private:
    uint64_t data_set_size;		// working set in Bytes
    uint64_t actual_ORAM_size;		// in Bytes

    int block_size;		// in Bytes
    int block_num_per_bucket;	// refer to Z value
    int64_t bucket_count;
    int64_t block_count;
    int64_t real_block_count;
    int64_t leaf_count;
    int level_count;
    bool debug;

    int64_t last_path;

    int64_t access_count;		
    int64_t actual_access_count;		
    int64_t dummy_access_count;		
    int64_t memory_access_count[2];

    int r_d_a_index;	// real 0 or dummy 1 access index
    int64_t path_read_count[2];		
    int64_t path_write_count[2];
    int64_t block_read_count[2][2];		
    int64_t block_write_count[2][2];

    int64_t stash_hit[2];		// [0]  real access £¬[1] dummy access 
    int64_t stash_miss[2];		// [0]  real access £¬[1] dummy access

    uint64_t hit_latency;
    uint64_t ready_latency;

    int hit_directly_cycles;
    int hit_through_mem_cycles;
    int remap_cycles;
    int write_back_cycles;

    int64_t hybrid_block_length_limit;

    double times;


    bool isOutPutLogFile;

    int64_t allocate_right_path_count;
    int64_t allocate_wrong_path_count;

public:

    enum Operations {
        read = 1,
        write = 2,
        write_back = 4,
        dummy = 8
    };

    Stash5 stash;

    bool* present;		
    int64_t* position_map;		
    int64_t* program_address;	
    int64_t* block_data;		
    int64_t* curPath_buffer;	

    int64_t* quantity_map;    

    int64_t* evict_queue;
    int* evict_queue_count;
    vector<int64_t> evict_backup_path;	

    int64_t max_freq;
    set<pair<int64_t, int64_t> > freq_cnt;

    default_random_engine random_engine2;
    uniform_int_distribution<int> distribute_int;

    PCDORAM();
    /*
        ds_s: data set size
        oram_s: ORAM size
        bl_s: block size
        bn_p: block num per bucket
        st_s: stash size
    */
    int configParameters(int64_t ds_s, int64_t oram_s, int bl_s, int bn_p, int st_s, bool isDebug);

    int generateRandomLeaf();

    void initialize();

    void setDefaultLatencyParas(int h_d, int h_t_m, int r, int w_b);

    int64_t getActualORAMsize();
    int getBlockSize();
    int getBlockNumPerBucket();
    int64_t getBlockCount();
    int64_t getBucketCount();
    int64_t getRealBlockCount();
    int64_t getLeafCount();
    int getLevelCount();
    int64_t* getPositionMap();

    int64_t getAccessCount();
    int64_t getActualAccessCount();
    int64_t getDummyAccessCount();
    int64_t getMemoryAccessCount();


    int64_t getRA_PathReadCount();
    int64_t getDA_PathReadCount();
    int64_t getRA_PathWriteCount();
    int64_t getDA_PathWriteCount();

    int64_t getRA_RealBlockReadCount();
    int64_t getDA_RealBlockReadCount();
    int64_t getRA_RealBlockWriteCount();
    int64_t getDA_RealBlockWriteCount();

    int64_t getRA_DummyBlockReadCount();
    int64_t getDA_DummyBlockReadCount();
    int64_t getRA_DummyBlockWriteCount();
    int64_t getDA_DummyBlockWriteCount();

    int64_t getStashHit();
    int64_t getStashMiss();

    int64_t getRA_MemoryAccessCount();
    int64_t getDA_MemoryAccessCount();

    int64_t getRA_StashHit();
    int64_t getDA_StashHit();
    int64_t getRA_StashMiss();
    int64_t getDA_StashMiss();

    uint64_t getHitLatency();
    uint64_t getReadyLatency();
    int64_t getAvgHitLatency();
    int64_t getAvgReadyLatency();

    int64_t getPathAllocateRightCount();
    int64_t getPathAllocateWrongCount();

    void resetMetric();
    void setDebug(bool debug);

    double feedbackTime();

   

    void displayPosMap();


    int64_t access(int64_t id, short operation, int64_t data);

    int64_t generateFromEvictBackupPath();

    int64_t readPath(int64_t interest, int64_t leaf_label, int64_t& index);

    bool scanStash(int64_t interest);		

    void remap(int64_t interest, int64_t new_leaf);

    void resetEvictQueue();

    int locateTheIntersectionForTmpArea(LocalCacheLine block, int64_t cur_pos);

    int findSpaceOfBucketOnPath(int cross_point);

    void pickBlockstoEvict(int64_t cur_pos);

    int64_t writePath(int leaf_label);

    int64_t backgroundEviction();

    void hybridBlockMerge();

    int locateTheIntersection(int64_t block_pos, int64_t cur_pos);

    int64_t hybridBlockKickOut(bool isHalf);

    void refreshQuantityMap(int cur_bucket, int numofPrev);

    void displayQuantityMap();

    void doSomeChange();

    int64_t findTheBestFitPathForEvict(int neededSpace, bool& isLarge);

    string formatedLogFileName(const string& trace_name,const string& cur_time);

    string getFormatedTime();

    void setIfLog(bool isLog);

    void resetFreqCnt();


    ~PCDORAM();
};