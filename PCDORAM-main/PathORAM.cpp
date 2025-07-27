#pragma once

#include <iostream>
#include <cmath>
#include <list>
#include <cassert>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include "include/PathORAM.h"
using namespace std;


PathORAM::PathORAM() { }

PathORAM::~PathORAM() { }

/*
    ds_s: data set size
    oram_s: ORAM size
    bl_s: block size
    bn_p: block num per bucket
    st_s: stash size
*/
int PathORAM::configParameters(uint64_t ds_s, uint64_t oram_s, int bl_s, int bn_p, int st_s, bool isDebug) {
   	if (ds_s > oram_s)
   		return 0;		
    cout << "configParameters: " << endl;
    cout << oram_s << " " << ds_s << endl;
    assert(oram_s > ds_s);	

    data_set_size = ds_s;
    actual_ORAM_size = oram_s;
    block_size = bl_s;
    block_num_per_bucket = bn_p;
    debug = isDebug;


    block_count = actual_ORAM_size / block_size;		
    real_block_count = ceil(data_set_size / block_size);	
    bucket_count = block_count / block_num_per_bucket;		

    level_count = ceil(log2(bucket_count + 1));		
    bucket_count = (1 << level_count) - 1;		
    block_count = bucket_count * block_num_per_bucket;		
    leaf_count = (bucket_count + 1) / 2;
    actual_ORAM_size = block_count * block_size;	


    assert(st_s > block_num_per_bucket * level_count); 
    stash.setMaxStashSize(st_s);	

    stash.setZvalue(block_num_per_bucket);
    stash.setLvalue(level_count);
    stash.setBlockSize(block_size);

    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    random_engine.seed(seed);
    uniform_int_distribution<int> distribute_int1(leaf_count - 1, bucket_count - 1);  
    distribute_int = distribute_int1;

    cout << "block_count: " << block_count << endl;
    cout << "real_block_count: " << real_block_count << endl;
    cout << "bucket_count: " << bucket_count << endl;
    cout << "level_count: " << level_count << endl;
    cout << "leaf_count: " << leaf_count << endl;
    cout << "actual_ORAM_size: " << actual_ORAM_size << endl;

    return 1;	
}

void PathORAM::setDefaultLatencyParas(int h_d, int h_t_m, int r, int w_b) {
    hit_directly_cycles = h_d;
    hit_through_mem_cycles = h_t_m;
    remap_cycles = r;
    write_back_cycles = w_b;
}

int PathORAM::generateRandomLeaf() {
    return (rand() % leaf_count) + bucket_count - leaf_count;
}

void PathORAM::initialize() {
    present = new bool[real_block_count + 1];		
    position_map = new int64_t[real_block_count + 1];
    program_address = new int64_t[block_count];
    curPath_buffer = new int64_t[level_count * block_num_per_bucket];
    evict_queue = new int64_t[level_count * block_num_per_bucket];  
    evict_queue_count = new int[level_count];
    block_data = new int64_t[block_count];
    assert(present && position_map && program_address && curPath_buffer && evict_queue && evict_queue_count);

    memset(present, 0, sizeof(bool) * (real_block_count + 1));		
    memset(program_address, -1, sizeof(int64_t) * block_count);	
    memset(block_data, -1, sizeof(int64_t) * block_count);

//	srand((unsigned)time(NULL));
    for (int64_t i = 0; i < real_block_count + 1; i++)		
    {
    //	int64_t rand_leaf = generateRandomLeaf();
        int64_t rand_leaf = distribute_int(random_engine);
        position_map[i] = rand_leaf;
    //	cout << position_map[i] << " --- +++ ";
    }
    cout << endl;

    resetMetric();
}

int64_t PathORAM::getActualORAMsize() { return actual_ORAM_size; }
int64_t PathORAM::getBlockCount() { return block_count; }
int64_t PathORAM::getBucketCount() { return bucket_count; }
int64_t PathORAM::getRealBlockCount() { return real_block_count; }
int64_t PathORAM::getLeafCount() { return leaf_count; }
int PathORAM::getBlockSize() { return block_size; }
int PathORAM::getBlockNumPerBucket() { return block_num_per_bucket; }
int PathORAM::getLevelCount() { return level_count; }
int64_t* PathORAM::getPositionMap() { return position_map; }
int64_t PathORAM::getAccessCount() { return access_count; }
int64_t PathORAM::getDummyAccessCount() { return dummy_access_count; }
int64_t PathORAM::getMemoryAccessCount() { return memory_access_count[0] + memory_access_count[1]; }
int64_t PathORAM::getActualAccessCount() { return actual_access_count; }


int64_t PathORAM::getRA_PathReadCount() { return path_read_count[0]; }
int64_t PathORAM::getDA_PathReadCount() { return path_read_count[1]; };
int64_t PathORAM::getRA_PathWriteCount() { return path_write_count[0]; }
int64_t PathORAM::getDA_PathWriteCount() { return path_write_count[1]; };

int64_t PathORAM::getRA_RealBlockReadCount() { return block_read_count[0][0]; }
int64_t PathORAM::getDA_RealBlockReadCount() { return block_read_count[1][0]; }
int64_t PathORAM::getRA_RealBlockWriteCount() { return block_write_count[0][0]; }
int64_t PathORAM::getDA_RealBlockWriteCount() { return block_write_count[1][0]; }

int64_t PathORAM::getRA_DummyBlockReadCount() { return block_read_count[0][1]; }
int64_t PathORAM::getDA_DummyBlockReadCount() { return block_read_count[1][1]; }
int64_t PathORAM::getRA_DummyBlockWriteCount() { return block_write_count[0][1]; }
int64_t PathORAM::getDA_DummyBlockWriteCount() { return block_write_count[1][1]; }


int64_t PathORAM::getStashHit() { return stash_hit[0] + stash_hit[1]; }
int64_t PathORAM::getStashMiss() { return stash_miss[0] + stash_miss[1]; }

int64_t PathORAM::getRA_MemoryAccessCount() { return memory_access_count[0]; }
int64_t PathORAM::getDA_MemoryAccessCount() { return memory_access_count[1]; }
int64_t PathORAM::getRA_StashHit() { return stash_hit[0]; }
int64_t PathORAM::getDA_StashHit() { return stash_hit[1]; }
int64_t PathORAM::getRA_StashMiss() { return stash_miss[0]; }
int64_t PathORAM::getDA_StashMiss() { return stash_miss[1]; }

uint64_t PathORAM::getHitLatency() { return hit_latency; }

uint64_t PathORAM::getReadyLatency() { return ready_latency; }

int64_t PathORAM::getAvgHitLatency() { return ceil(hit_latency * 1.0 / access_count); }

int64_t PathORAM::getAvgReadyLatency() { return ceil(ready_latency * 1.0 / access_count); }

void PathORAM::resetMetric() {
    
    access_count = 0;
    actual_access_count = 0;
    dummy_access_count = 0;

	r_d_a_index = 0;
	for (int i = 0; i < 2; i++) {
		stash_hit[i] = 0;
		stash_miss[i] = 0;
		memory_access_count[i] = 0;

		path_read_count[i] = 0;
		path_write_count[i] = 0;
		for (int j = 0; j < 2; j++) {
			block_read_count[i][j] = 0;
			block_write_count[i][j] = 0;
		}
	}

    hit_latency = 0;
    ready_latency = 0;
}

void PathORAM::setDebug(bool debug) { this->debug = debug; }

double PathORAM::feedbackTime() { return 0.0; }



void PathORAM::displayPosMap() {
    for (int64_t i = 0; i < real_block_count; i++)
        cout << position_map[i] << " ";
    cout << endl;
}

int64_t PathORAM::access(int64_t id, short operation, int64_t data) {
    if (id < 0)
        id = real_block_count;  // id < 0 : dummy_access
    //assert(id < real_block_count + 1);
	if (id >= real_block_count + 1)
		id = rand() % real_block_count;
    assert(!stash.isFull() || (operation & dummy));  

	r_d_a_index = (operation == dummy) ? 1 : 0;

	access_count++; 
	if (id == real_block_count)
		dummy_access_count++;
	else
		actual_access_count++;

    if (operation & write_back) {		// this block was evicted from LLC, append it to stash without performing the ORAM access
        assert(!present[id]);      
        stash.local_cache.push_back(LocalCacheLine(id, position_map));
        present[id] = true;
        cout << "Block is evicted from LLC." << endl;
        return 0;		
    }

    resetEvictQueue();

    int64_t IO_traffic = 0;
    int64_t cur_pos, new_pos;

    srand((unsigned)time(NULL));
    cur_pos = position_map[id];		// gain current leaf that mapped
    do {
        new_pos = distribute_int(random_engine);
    } while (new_pos == cur_pos);

    bool isExist_pre = scanStash(id);
    if (isExist_pre) {		
        hit_latency += hit_directly_cycles;
        if (debug)
            cout << "Block that requested has found in the stash. No need to access ORAM." << endl;
            stash_hit[r_d_a_index]++;
    }
    else { 
        memory_access_count[r_d_a_index]++;
            stash_miss[r_d_a_index]++;

        if (debug)
            cout << "Block hasn't be found. Accessing ORAM..." << endl;

        int64_t index = 0;		
        IO_traffic += readPath(id, cur_pos, index);		
		path_read_count[r_d_a_index]++;

        if (!present[id]) {		
            if (operation & read) {
                if (debug)
                    cout << "ERROR! Reading non-existent block..." << endl;
            } else if (operation & write) {		
                present[id] = true;
                stash.local_cache.push_back(LocalCacheLine(id, position_map));
                if (debug)
                    cout << "Creating a new block..." << endl;
            }
        } else {	// block exists
            if (operation & read) {
                if (debug)
                    cout << "Reading the requested block from ORAM tree..." << endl;
            } else if (operation & write) {		// modify the block data
                block_data[index] = data;	
                if (debug)
                    cout << "Updating the block data..." << endl;
            }
        }
    }
   
    stash.updatePeakAndLastOccupancy();		

    remap(id, new_pos);

    pickBlockstoEvict(cur_pos);
    IO_traffic += writePath(cur_pos);
	path_write_count[r_d_a_index]++;

    return IO_traffic;
}

int64_t PathORAM::readPath(int64_t interest, int64_t leaf_label, int64_t &index) {
    if (debug)
        cout << "Read Phase - interest: " << interest << endl;
    int64_t bucket_index = leaf_label;

    for (int i = 0; i < level_count; i++) {		
        assert(bucket_index || (bucket_index == 0 && i == level_count - 1));
        for (int j = 0; j < block_num_per_bucket; j++) {
            int64_t id = program_address[bucket_index * block_num_per_bucket + j];
            if (id != -1) {  
				block_read_count[r_d_a_index][0]++;
                if (debug)
                    cout << "read in id: " << id << " ";
                stash.local_cache.push_back(LocalCacheLine(id, position_map));
                program_address[bucket_index * block_num_per_bucket + j] = -1; 
			} else
				block_read_count[r_d_a_index][1]++;
            if (id == interest)
                index = bucket_index * block_num_per_bucket + j;
        }
        if (bucket_index == 1)
            bucket_index = 0;
        else
            bucket_index = ceil((bucket_index - 2) * 1.0 / 2);		
    }
    hit_latency += hit_through_mem_cycles * 1ll * level_count * block_num_per_bucket; 
    return 1ll * level_count * block_num_per_bucket;
}

bool PathORAM::scanStash(int64_t interest) {	
    bool isFound = false;
    for (stash.iter = stash.local_cache.begin(); stash.iter != stash.local_cache.end(); stash.iter++) {
        if ((stash.iter)->id == interest) {
            isFound = true;
            break;
        }
    }
    return isFound;
}

void PathORAM::remap(int64_t interest, int64_t new_leaf) { 
    position_map[interest] = new_leaf;
    ready_latency += remap_cycles;
}

void PathORAM::resetEvictQueue() {
    memset(evict_queue, -1, sizeof(int64_t) * level_count * block_num_per_bucket);	
    memset(evict_queue_count, 0, sizeof(int) * level_count);
}

int PathORAM::locateTheIntersection(LocalCacheLine block, int64_t cur_pos) {
    assert(block.id >= 0);
    int64_t block_pos = *(block.position); 

    if (block_pos > cur_pos)		
        swap(block_pos, cur_pos);

    if (cur_pos - block_pos == 0)
        return level_count;

    int64_t mostLeftLeaf_index = (bucket_count - 1) / 2;
    int64_t mostRightLeaf_index = bucket_count - 1;

    int64_t left = mostLeftLeaf_index;
    int64_t right = mostRightLeaf_index;
    int64_t mid;
    int intersection = 0;
    while (intersection <= level_count) {
        intersection++;
        mid = (right - left) / 2 + 1;
        if (block_pos <= right - mid && cur_pos >= left + mid)
            return intersection;
        else if (cur_pos <= right - mid)		
            right -= mid;
        else if (block_pos >= left + mid)		
            left += mid;
    }
    return -1;
}

int PathORAM::findSpaceOfBucketOnPath(int cross_point) {
    for (int i = cross_point - 1; i >= 0; i--)
        if (evict_queue_count[i] < block_num_per_bucket)
            return i;
    return -1;
}

void PathORAM::pickBlockstoEvict(int64_t cur_pos) {
    int intersection = -1;
    int deepestNode = -1;
    stash.iter = stash.local_cache.begin();
    while (stash.iter != stash.local_cache.end()) {
        LocalCacheLine block = *stash.iter;
        intersection = locateTheIntersection(block, cur_pos);
        deepestNode = findSpaceOfBucketOnPath(intersection);
        if (deepestNode != -1) {
            evict_queue[deepestNode * block_num_per_bucket + evict_queue_count[deepestNode]] = block.id;  
            evict_queue_count[deepestNode]++;
            stash.iter = stash.local_cache.erase(stash.iter);		
        }
        else
            stash.iter++;
    }
}

int64_t PathORAM::writePath(int leaf_label) {
    int64_t traffic = 0;
    int64_t bucket_index = leaf_label;
    for (int i = 0; i < level_count; i++) {		
        assert(bucket_index || (bucket_index == 0 && i == level_count - 1));
        for (int j = 0; j < block_num_per_bucket; j++) {
            traffic++;
            int64_t id = evict_queue[(level_count - 1 - i) * block_num_per_bucket + j];		
			if (id == -1)
				block_write_count[r_d_a_index][1]++;
			else 
				block_write_count[r_d_a_index][0]++;
            
            program_address[bucket_index * block_num_per_bucket + j] = id;
        }
        if (bucket_index == 1)
            bucket_index = 0;
        else
            bucket_index = ceil((bucket_index - 2) * 1.0 / 2);		
    }
    ready_latency += write_back_cycles * 1ll * level_count * block_num_per_bucket;
    return traffic;
}

int64_t PathORAM::backgroundEviction() {
    int64_t traffic = 0;
    while (stash.isAlmostFull()) {
        if (debug)
            cout << "Background eviction..." << endl;
        traffic += access(real_block_count, PathORAM::dummy, -1);
    }
    return traffic;
}