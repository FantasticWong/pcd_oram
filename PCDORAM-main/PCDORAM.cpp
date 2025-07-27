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
#include <unordered_map>
#include <sstream>
#include "include/PCDORAM.h"
#include <cstdio>
using namespace std;

PCDORAM::PCDORAM() {
    isOutPutLogFile = false;
}



PCDORAM::~PCDORAM() { }

/*
    ds_s: data set size
    oram_s: ORAM size
    bl_s: block size
    bn_p: block num per bucket
    st_s: stash size
*/
int PCDORAM::configParameters(int64_t ds_s, int64_t oram_s, int bl_s, int bn_p, int st_s, bool isDebug) {	
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
    actual_ORAM_size = block_count * block_size;	// in Byte

    assert(st_s > block_num_per_bucket * level_count);
    stash.setMaxStashSize(st_s);	

    stash.setZvalue(block_num_per_bucket);
    stash.setL(level_count);
    stash.setBlocksize(block_size);

    unsigned seed = chrono::system_clock::now().time_since_epoch().count();
    random_engine2.seed(seed);
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

int PCDORAM::generateRandomLeaf() {
    return distribute_int(random_engine2);
}

void PCDORAM::initialize() {
    present = new bool[real_block_count + 1];		
    position_map = new int64_t[real_block_count + 1];
    program_address = new int64_t[block_count];
    curPath_buffer = new int64_t[level_count * block_num_per_bucket];
    evict_queue = new int64_t[level_count * block_num_per_bucket];
    evict_queue_count = new int[level_count];
    block_data = new int64_t[block_count];
    quantity_map = new int64_t[leaf_count];  
    assert(present && position_map && program_address && curPath_buffer && evict_queue && evict_queue_count);


    if (isOutPutLogFile) {
        string cur_time = getFormatedTime();
        string log_file_name = formatedLogFileName("fmm", cur_time);
        freopen(log_file_name.c_str(), "w", stdout);
    }

    allocate_right_path_count = 0;
    allocate_wrong_path_count = 0;


    memset(present, 0, sizeof(bool) * (real_block_count + 1));		
    memset(program_address, -1, sizeof(int64_t) * block_count);	
    memset(block_data, -1, sizeof(int64_t) * block_count);
    memset(quantity_map, 0, sizeof(int64_t) * (leaf_count));  

    for (int64_t i = 0; i < real_block_count + 1; i++) {		
        //	int64_t rand_leaf = generateRandomLeaf();
        int64_t rand_leaf = distribute_int(random_engine2);
        position_map[i] = rand_leaf;
        //	cout << position_map[i] << " --- +++ ";
    }
    cout << endl;

    times = 0.0;

    last_path = -1;

    resetMetric();
}

void PCDORAM::setDefaultLatencyParas(int h_d, int h_t_m, int r, int w_b) {
    hit_directly_cycles = h_d;
    hit_through_mem_cycles = h_t_m;
    remap_cycles = r;
    write_back_cycles = w_b;
}



int64_t PCDORAM::getActualORAMsize() { return actual_ORAM_size; }
int PCDORAM::getBlockSize() { return block_size; }
int PCDORAM::getBlockNumPerBucket() { return block_num_per_bucket; }
int64_t PCDORAM::getBlockCount() { return block_count; }
int64_t PCDORAM::getBucketCount() { return bucket_count; }
int64_t PCDORAM::getRealBlockCount() { return real_block_count; }
int64_t PCDORAM::getLeafCount() { return leaf_count; }
int PCDORAM::getLevelCount() { return level_count; }
int64_t PCDORAM::getAccessCount() { return access_count; }
int64_t PCDORAM::getDummyAccessCount() { return dummy_access_count; }
int64_t* PCDORAM::getPositionMap() { return position_map; }
int64_t PCDORAM::getMemoryAccessCount() { return memory_access_count[0] + memory_access_count[1]; }
int64_t PCDORAM::getActualAccessCount() { return actual_access_count; }


int64_t PCDORAM::getRA_PathReadCount() { return path_read_count[0]; }
int64_t PCDORAM::getDA_PathReadCount() { return path_read_count[1]; };
int64_t PCDORAM::getRA_PathWriteCount() { return path_write_count[0]; }
int64_t PCDORAM::getDA_PathWriteCount() { return path_write_count[1]; };

int64_t PCDORAM::getRA_RealBlockReadCount() { return block_read_count[0][0]; }
int64_t PCDORAM::getDA_RealBlockReadCount() { return block_read_count[1][0]; }
int64_t PCDORAM::getRA_RealBlockWriteCount() { return block_write_count[0][0]; }
int64_t PCDORAM::getDA_RealBlockWriteCount() { return block_write_count[1][0]; }

int64_t PCDORAM::getRA_DummyBlockReadCount() { return block_read_count[0][1]; }
int64_t PCDORAM::getDA_DummyBlockReadCount() { return block_read_count[1][1]; }
int64_t PCDORAM::getRA_DummyBlockWriteCount() { return block_write_count[0][1]; }
int64_t PCDORAM::getDA_DummyBlockWriteCount() { return block_write_count[1][1]; }


int64_t PCDORAM::getStashHit() { return stash_hit[0] + stash_hit[1]; }
int64_t PCDORAM::getStashMiss() { return stash_miss[0] + stash_miss[1]; }

int64_t PCDORAM::getRA_MemoryAccessCount() { return memory_access_count[0]; }
int64_t PCDORAM::getDA_MemoryAccessCount() { return memory_access_count[1]; }
int64_t PCDORAM::getRA_StashHit() { return stash_hit[0]; }
int64_t PCDORAM::getDA_StashHit() { return stash_hit[1]; }
int64_t PCDORAM::getRA_StashMiss() { return stash_miss[0]; }
int64_t PCDORAM::getDA_StashMiss() { return stash_miss[1]; }

uint64_t PCDORAM::getHitLatency() { return hit_latency; }

uint64_t PCDORAM::getReadyLatency() { return ready_latency; }

int64_t PCDORAM::getAvgHitLatency() { return ceil(hit_latency * 1.0 / access_count); }

int64_t PCDORAM::getAvgReadyLatency() { return ceil(ready_latency * 1.0 / access_count); }

int64_t PCDORAM::getPathAllocateRightCount() { return allocate_right_path_count; }

int64_t PCDORAM::getPathAllocateWrongCount() { return allocate_wrong_path_count; }

void PCDORAM::resetMetric() {

    access_count = 0;			
    dummy_access_count = 0;
    actual_access_count = 0;
    max_freq = 0;

    hybrid_block_length_limit = ceil(0.4 * level_count * block_num_per_bucket);

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
void PCDORAM::setDebug(bool debug) { this->debug = debug; }

double PCDORAM::feedbackTime() { return times; }

void PCDORAM::displayPosMap() {
    for (int64_t i = 0; i < real_block_count; i++)
        cout << position_map[i] << " ";
    cout << endl;
}

int64_t PCDORAM::access (int64_t id, short operation, int64_t data) {
    if (id < 0)
        id = real_block_count;

    if (debug) {
        cout << "real_block_count: " << real_block_count << "-------------" << "Interest Block: " << id << endl;
    }
        
    if (id >= real_block_count + 1)
        id = rand() % real_block_count;

    assert(!stash.isFull() || (operation & dummy));

    r_d_a_index = (operation == dummy) ? 1 : 0;

    access_count++;
    if (id == real_block_count)
        dummy_access_count++;
    else
        actual_access_count++;

    if (operation & write_back) {		
        stash.putIntoCandidateArea(LocalCacheLine(id, position_map));
        present[id] = true;
        if (debug)
            cout << "Block is evicted from LLC." << endl;
        return 0;		
    }

    resetEvictQueue();

    int64_t IO_traffic = 0;
    int64_t cur_pos, new_pos;

    cur_pos = position_map[id];		// gain current leaf that mapped
    do {
        new_pos = distribute_int(random_engine2);
    } while (new_pos == cur_pos);
    if (debug)
        cout << "cur_pos: " << cur_pos << " new_pos: " << new_pos << endl;

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
        evict_backup_path.push_back(cur_pos);		

        last_path = cur_pos;

        if (debug)
            cout << "temp size: " << stash.temporal_area.size() << endl;

        if (!present[id]) {			// new block, not currently in ORAM
            if (operation & read) {
                if (debug)
                    cout << "ERROR! Reading non-existent block..." << endl;
            }
            else if (operation & write) {		// create a new block and append into stash
                present[id] = true;
                stash.putIntoCandidateArea(LocalCacheLine(id, position_map));

                if (debug)
                    cout << "Creating a new block..." << endl;
            }
        }
        else {	// block exists
            if (operation & read) {
                if (debug)
                    cout << "Reading the requested block from ORAM tree..." << endl;
            }
            else if (operation & write) {		
                block_data[index] = data;	
                if (debug)
                    cout << "Updating the block data..." << endl;
            }
        }
    }
    stash.updatePeakAndLastOccupancy();		// record the infomation of stash's occupancy

    remap(id, new_pos);  

    if (isExist_pre)
        return IO_traffic;

    if (debug)
        cout << "stash size before: " << stash.getCurrentStashSize() << endl;

    if (operation & dummy) {

    }

    if (stash.isAlmostFull()) {
        if (debug) {
            cout << "isAlmostFull" << endl;
            cout << "isTemporalAreaAlmostFull" << endl;
            cout << "stash.tempsize before: " << stash.temporal_area.size() << endl;
        }

        pickBlockstoEvict(cur_pos);
        IO_traffic += writePath(cur_pos);
        path_write_count[r_d_a_index]++;
        if (debug)
            cout << "stash.tempsize after: " << stash.temporal_area.size() << endl;

        if (stash.isAlmostFull() && stash.candidate_area_key.size() > level_count) {
            if (debug) {
                cout << "isAlmostFull" << endl;
                cout << "stash.cansize before: " << stash.candidate_area_key.size() << endl;
                cout << "Merging hybrid blocks..." << endl;
            }
            hybridBlockMerge();
            if (debug)
                cout << "Kicking out hybrid blocks..." << endl;
            IO_traffic += hybridBlockKickOut(true);
            path_write_count[r_d_a_index]++;       

            if (debug) {
                cout << "Finishing kicking out..." << endl;
                cout << "candidate_area size after: " << stash.candidate_area_key.size() << endl;
            }
        }

    }

    if (debug)
        cout << "stash size after: " << stash.getCurrentStashSize() << endl;
    return IO_traffic;
}

int64_t PCDORAM::generateFromEvictBackupPath() {
    if (evict_backup_path.size() == 0)
        return distribute_int(random_engine2);
    int64_t leaf_index = (rand() % evict_backup_path.size());
    int64_t leaf = evict_backup_path[leaf_index];
    vector<int64_t>::iterator it = evict_backup_path.begin() + leaf_index;
    evict_backup_path.erase(it);	
    return leaf;
}

int64_t PCDORAM::readPath(int64_t interest, int64_t leaf_label, int64_t& index) {
    if (debug) {
        cout << "Read Phase - interest block: " << interest << endl;
        cout << "Before read, currentStashsize: "<< stash.getCurrentStashSize() << "_+_+_+_+__+_+___+_+++" << endl;
    }
    int cross_layer = 0;

    int64_t bucket_index = leaf_label;
    for (int i = cross_layer; i < level_count; i++) {		

        assert(bucket_index || (bucket_index == 0 && i == level_count - 1));
        for (int j = 0; j < block_num_per_bucket; j++) {
            int64_t id = program_address[bucket_index * block_num_per_bucket + j];
            if (id == interest) {		
                block_read_count[r_d_a_index][0]++;
                index = bucket_index * block_num_per_bucket + j;
                stash.putIntoCandidateArea(LocalCacheLine(id, position_map));
                program_address[bucket_index * block_num_per_bucket + j] = -1;
            }
            else if (id != -1) {
                block_read_count[r_d_a_index][0]++;
                stash.putIntoTemporalArea(LocalCacheLine(id, position_map));
                program_address[bucket_index * block_num_per_bucket + j] = -1;
            }
            else
                block_read_count[r_d_a_index][1]++;
        }
        if (bucket_index == 1)
            bucket_index = 0;
        else
            bucket_index = ceil((bucket_index - 2) * 1.0 / 2);		
    }
    if (debug)
        cout << "After read, currentStashsize: " << stash.getCurrentStashSize() << "_+_+_+_+__+_+___+_+++" << endl;
    hit_latency += hit_through_mem_cycles * 1ll * (level_count - cross_layer) * block_num_per_bucket;
    return 1ll * (level_count - cross_layer) * block_num_per_bucket;
}

bool PCDORAM::scanStash(int64_t interest) {		
    bool isFound = false;
    if (stash.candidate_area_key.find(interest) != stash.candidate_area_key.end()) {
        stash.putIntoCandidateArea(LocalCacheLine(interest, position_map));
        isFound = true;
        return isFound;
    }

    if (stash.temporal_area.find(interest) != stash.temporal_area.end()) {
        isFound = true;
        stash.putIntoCandidateArea(LocalCacheLine(interest, position_map));
        stash.temporal_area.erase(interest);
        return isFound;
    }

    return isFound;
}

void PCDORAM::remap(int64_t interest, int64_t new_leaf) {
    position_map[interest] = new_leaf;
    ready_latency += remap_cycles;
}

void PCDORAM::resetEvictQueue() {
    memset(evict_queue, -1, sizeof(int64_t) * level_count * block_num_per_bucket);	
    memset(evict_queue_count, 0, sizeof(int) * level_count);
}

int PCDORAM::locateTheIntersectionForTmpArea(LocalCacheLine block, int64_t cur_pos) {
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

int PCDORAM::findSpaceOfBucketOnPath(int cross_point) {
    for (int i = cross_point - 1; i >= 0; i--)
        if (evict_queue_count[i] < block_num_per_bucket)  
            return i;
    return -1;
}

void PCDORAM::pickBlockstoEvict(int64_t cur_pos) {
    int intersection = -1;
    int deepestNode = -1;
    auto tem_iter = stash.temporal_area.begin();
    while (tem_iter != stash.temporal_area.end()) {
        LocalCacheLine block = (*tem_iter).second;
        intersection = locateTheIntersectionForTmpArea(block, cur_pos);
        deepestNode = findSpaceOfBucketOnPath(intersection);
        if (deepestNode != -1) { 
            evict_queue[deepestNode * block_num_per_bucket + evict_queue_count[deepestNode]] = block.id;
            evict_queue_count[deepestNode]++;
            tem_iter = stash.temporal_area.erase(tem_iter);		
        }
        else
            tem_iter++;
    }
}

int64_t PCDORAM::writePath(int leaf_label) {
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

int64_t PCDORAM::backgroundEviction() {
    int64_t traffic = 0;
    while (stash.isAlmostFull()) {
        if (debug)
            cout << "Background eviction..." << endl;
        traffic += access(real_block_count, PCDORAM::dummy, -1);
    }
    return traffic;
}

void PCDORAM::hybridBlockMerge() {

    freq_cnt.clear();  

    if (debug) {
        cout << "before merge: ";
        for (auto& ele : stash.candidate_area_key) {
            cout << "(" << stash.candidate_area_key_freq[ele.second->id] << ", " << ele.second->id << ", " << *(ele.second->position) << ")";
        }
        cout << endl;

        cout << "stash.candidate_area_key.size: " << stash.candidate_area_key.size() << endl;
        cout << "stash.candidate_area_freq.size: " << stash.candidate_area_freq.size() << endl;
        for (auto& f_ele : stash.candidate_area_freq) {
            cout << "f_ele.size: " << f_ele.second.size() << " ";
        }
        cout << endl;
    }

    max_freq = 0;

    for (auto& f_ele : stash.candidate_area_freq) {
        freq_cnt.insert(make_pair(f_ele.first, f_ele.second.size()));
        max_freq = max(max_freq, f_ele.first);
    }

}

int PCDORAM::locateTheIntersection(int64_t block_pos, int64_t cur_pos) {
    if (block_pos > cur_pos)		
        swap(block_pos, cur_pos);

    if (cur_pos - block_pos == 0)
        return level_count;

    int64_t mostLeftLeaf_index = (bucket_count - 1) / 2;
    int64_t mostRightLeaf_index = bucket_count - 1;
    int64_t gap = leaf_count;

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


int64_t PCDORAM::hybridBlockKickOut(bool isHalf) {
    
    int64_t traffic = 0;
    int cnt = 0;		
    int erase_cnt = 0;

    int needed_place = stash.candidate_area_key.size();   
    int erase_count = 0;

    //int freq_cnt_size = freq_cnt.size();
    //int evict_freq_size = 0;
    
    for (auto f_iter = freq_cnt.begin(); f_iter != freq_cnt.end();) {
        


        pair<int64_t, int64_t> ele = *f_iter;
        int64_t freq = ele.first;
        int64_t f_cnt = ele.second;

        vector<pair<int64_t, int64_t>> space;		// <target_left, index>
        int cur_needed_place = f_cnt; 
        int64_t cur_erase_cnt = 0; 
        
        bool isFind = false;
        int64_t target_leaf = findTheBestFitPathForEvict(f_cnt,isFind); 
        if (isFind) { 
            allocate_right_path_count++;
        }
        else {
            allocate_wrong_path_count++;
        }
        int bucket_index = target_leaf;


        while (cur_needed_place > 0) { 
            bucket_index = target_leaf;
            for (int i = 0; i < level_count; i++) {  
                assert(bucket_index || (bucket_index == 0 && i == level_count - 1));
                for (int j = 0; j < block_num_per_bucket; j++) {
                    int64_t t_id = program_address[bucket_index * block_num_per_bucket + j];
                    if (t_id != -1)
                        continue;
                    space.emplace_back(make_pair(target_leaf, bucket_index * block_num_per_bucket + j));
                    program_address[bucket_index * block_num_per_bucket + j] = 1; 
                    if (cur_needed_place == 0)
                        break;
                }
                if (bucket_index == 1)
                    bucket_index = 0;
                else
                    bucket_index = ceil((bucket_index - 2) * 1.0 / 2);		
                if (cur_needed_place == 0)
                    break;
            }
            if (cur_needed_place > 0) {
                target_leaf = findTheBestFitPathForEvict(cur_needed_place, isFind);
                if (isFind) {
                    allocate_right_path_count++;
                }
                else {
                    allocate_wrong_path_count++;
                }
            }
        }


        int i = 0;
        auto data_list = stash.candidate_area_freq[freq];
        for (auto iter = data_list.begin(); iter != data_list.end(); iter++) {
            int64_t block_id = iter->id;
            program_address[space[i].second] = block_id;
            ready_latency += write_back_cycles;
            remap(block_id, space[i].first);
            stash.candidate_area_key.erase(block_id);
            stash.candidate_area_key_freq.erase(block_id);  
            i++;

            cnt++;
            block_write_count[r_d_a_index][0]++;
        }

        stash.candidate_area_freq[freq].clear();
        stash.candidate_area_freq.erase(freq);
        f_iter = freq_cnt.erase(f_iter);  
    }


    if (freq_cnt.size() == 0)
        max_freq = 0;
    else {
        max_freq = freq_cnt.rbegin()->second;
    }

    traffic = cnt;
    return traffic;
}

void PCDORAM::refreshQuantityMap(int cur_bucket, int numofPrev) {

    if (cur_bucket >= (leaf_count - 1)) {
        for (int i = 0; i < block_num_per_bucket; i++) {
            int64_t id = program_address[cur_bucket * block_num_per_bucket + i];
            if (id == -1)  
                numofPrev++;
        }
        quantity_map[cur_bucket - leaf_count + 1] = numofPrev;

    }
    else {  
        for (int i = 0; i < block_num_per_bucket; i++) {
            int64_t id = program_address[cur_bucket * block_num_per_bucket + i];
            if (id == -1)
                numofPrev++;
        }
        refreshQuantityMap(cur_bucket * 2 + 1, numofPrev);
        refreshQuantityMap(cur_bucket * 2 + 2, numofPrev);
    }
}

void PCDORAM::displayQuantityMap() {
    int num = 0;
    for (int i = 0; i < leaf_count; i++) {
        cout << "leaf" << i << "  :" << quantity_map[i] <<";  ";
        num = num++;
        if (num == 8) {
            cout << endl;
            num = 0;
        }
    }
    cout << endl;
}

void PCDORAM::doSomeChange()
{
    program_address[0] = 11;
    program_address[5] = 212;
    program_address[64] = 55;
    program_address[566] = 44;
    program_address[1522] = 444;
}

int64_t PCDORAM::findTheBestFitPathForEvict(int neededSpace,bool& isLarge)
{
    refreshQuantityMap(0, 0);  

    int64_t targetPathForBig = 0; 
    int64_t targetPathForSmall = 0;
    int64_t minGapForBig = INT64_MAX;
    int64_t minGapForSmall = INT64_MAX;
    isLarge = false;

    for (int64_t i = 0; i < leaf_count; i++) {
        if (neededSpace - quantity_map[i] == 0){
            return i + leaf_count - 1; 
        }
        if (quantity_map[i] > neededSpace) {
            isLarge = true;
            if ((quantity_map[i] - neededSpace) < minGapForBig) {
                targetPathForBig = i;
                minGapForBig = quantity_map[i] - neededSpace;
            }
        }
        if (quantity_map[i] < neededSpace && !isLarge) {
            if ((neededSpace - quantity_map[i]) < minGapForSmall) {
                targetPathForSmall = i;
                minGapForSmall = neededSpace - quantity_map[i];
            }
        }
    }

    if (isLarge)
        return (targetPathForBig + leaf_count - 1);
    else
        return (targetPathForSmall + leaf_count - 1);
}


string PCDORAM::formatedLogFileName(const string& trace_name, const string &cur_time) {
    string log_folder = "log/";
    const string post_prefix = ".txt";
    const string connector = "-";
    string output_filepath;
    output_filepath = log_folder + trace_name + connector + "PCDORAM" + connector + cur_time + post_prefix;
    return output_filepath;
}

string PCDORAM::getFormatedTime() {
    time_t cur_time;
    struct tm* time_info;
    time(&cur_time);
    time_info = localtime(&cur_time);

    char* time_buf = new char[12];
    sprintf(time_buf, "[%02d-%02d-%02d]", time_info->tm_hour, time_info->tm_min, time_info->tm_sec);
    string time_str(time_buf);
    delete(time_buf);
    time_buf = NULL;

    return time_str;
}

void PCDORAM::setIfLog(bool isLog)
{
    isOutPutLogFile = isLog;
}

void PCDORAM::resetFreqCnt()
{
    freq_cnt.clear();
}
