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
#include "LocalCacheLine.h"
#include "Stash.h"
using namespace std;


class PathORAM {
private:
	uint64_t data_set_size;		
	uint64_t actual_ORAM_size;		

	int block_size;		
	int block_num_per_bucket;	
	int64_t bucket_count;
	int64_t block_count;
	int64_t real_block_count;
	int64_t leaf_count;
	int level_count;
	bool debug;

	int64_t access_count;		
	int64_t actual_access_count;		
	int64_t dummy_access_count;		
	int64_t memory_access_count[2];	

	int r_d_a_index;	
	int64_t path_read_count[2];		
	int64_t path_write_count[2];
	int64_t block_read_count[2][2];		
	int64_t block_write_count[2][2];

	int64_t stash_hit[2];		
	int64_t stash_miss[2];		

	uint64_t hit_latency;
	uint64_t ready_latency;

	int hit_directly_cycles;
	int hit_through_mem_cycles;
	int remap_cycles;
	int write_back_cycles;

public:

	enum Operations {
		read = 1,
		write = 2,
		write_back = 4,
		dummy = 8
	};

	Stash stash;

	bool *present;		
	int64_t *position_map;		
	int64_t *program_address;	
	int64_t *block_data;		
	int64_t *curPath_buffer;	

	int64_t *evict_queue;
	int *evict_queue_count;

	default_random_engine random_engine;  
	uniform_int_distribution<int> distribute_int; 

	PathORAM();
	/*
		ds_s: data set size
		oram_s: ORAM size
		bl_s: block size
		bn_p: block num per bucket
		st_s: stash size
	*/
	int configParameters(uint64_t ds_s, uint64_t oram_s, int bl_s, int bn_p, int st_s, bool isDebug);
	void initialize();

	void setDefaultLatencyParas(int h_d, int h_t_m, int r, int w_b);

	int generateRandomLeaf();

	int64_t getActualORAMsize();
	int64_t getBlockCount();
	int64_t getBucketCount();
	int64_t getRealBlockCount();
	int64_t getLeafCount();
	int getBlockSize();
	int getBlockNumPerBucket();
	int getLevelCount();

	int64_t *getPositionMap();

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

	void resetMetric();
	void setDebug(bool debug);

	double feedbackTime();

	void displayPosMap();

	
	int64_t access(int64_t id, short operation, int64_t data);

	int64_t readPath(int64_t interest, int64_t leaf_label, int64_t &index);

	bool scanStash(int64_t interest);

	void remap(int64_t interest, int64_t new_leaf);

	void resetEvictQueue();

	int locateTheIntersection(LocalCacheLine block, int64_t cur_pos);

	int findSpaceOfBucketOnPath(int cross_point);

	void pickBlockstoEvict(int64_t cur_pos);

	int64_t writePath(int leaf_label);

	int64_t backgroundEviction();

	~PathORAM();
};