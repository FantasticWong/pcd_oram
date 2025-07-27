#pragma once


#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <algorithm>
#include "PCDORAM.h"

class HierachicalPCDORAM {
private:
    int max_hierarchy;	
    int hierarchy;		

    uint64_t* data_size;
    double* utilization;	
    int* block_size;
    int* block_num_per_bucket;
    uint64_t* bucket_count;
    uint64_t* block_count;
    uint64_t* leaf_count;
    int* level_count;
    bool debug;

    int* position_map_scale_factor;
    uint64_t final_position_map_size;

    int64_t access_count;
    int64_t dummy_access_count;

    int64_t* address;

    int max_stash_size;
public:

    PCDORAM** hier_PCDORAM;

    HierachicalPCDORAM();

    /*
        ds_s: data size
        util: utilization
        HOram_bl_s: Hierarchical Path ORAM block size
        HOram_bn_p: Hierarchical Path ORAM block num per bucket
        maxPosMap_size: the specified on-chip storage
        st_s: stash sized
    */
    int configParameters(uint64_t ds_s, const double* util, int* HOram_bl_s, int* HOram_bn_p, uint32_t maxPosMap_size, int st_s, bool isDebug);

    void initialize();

    void setDefaultLatencyParas(int h_d, int h_t_m, int r, int w_b);

    int64_t getMergeTimes() { return 0; };

    int64_t getRealBlockCountForHierORAM();

    void resetMetricForHierORAM();

    void setDebug(bool debug);

    double feedbackTime();

    int64_t getStashHitForHierORAM();

    int64_t getStashMissForHierORAM();

    int getHierarchy();
    int getBlockSize(int index);
    int getBlockNumPerBucket(int index);
    int64_t getBlockCount(int index);
    int64_t getBucketCount(int index);
    int64_t getPosMapScaleFactor(int index);
    int64_t getLeafCount(int index);
    int getLevelCount(int index);

    int getRealBlockCountOfDataORAM();

    int64_t getAccessCount();
    int64_t getActualAccessCount();
    int64_t getDummyAccessCount();
    int64_t getMemoryAccessCount();

    int64_t getAccessCountOfDataORAM();
    int64_t getActualAccessCountOfDataORAM();
    int64_t getDummyAccessCountOfDataORAM();
    int64_t getMemoryAccessCountOfDataORAM();

    int64_t getRA_MemoryAccessCount();
    int64_t getDA_MemoryAccessCount();
    int64_t getRA_StashHitCount();
    int64_t getDA_StashHitCount();
    int64_t getRA_MemoryAccessCountOfDataORAM();
    int64_t getDA_MemoryAccessCountOfDataORAM();
    int64_t getRA_StashHitCountOfDataORAM();
    int64_t getDA_StashHitCountOfDataORAM();

    // RA: real access£¬DA:dummy access
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

    uint64_t getHitLatency();
    uint64_t getReadyLatency();
    int64_t getAvgHitLatency();
    int64_t getAvgReadyLatency();

    int64_t getPathAllocateRightCountOfDataORAM();
    int64_t getPathAllocateWrongCountOfDataORAM();

  
    void displayPosMapOfDataORAM();

    void generateAddress(int64_t addr);

    uint64_t access(int64_t id, short operation, int64_t data);

    bool isLocalcacheFull();

    uint64_t backgroundEviction();

    ~HierachicalPCDORAM();
};
