#pragma once

#include <cassert>
#include <cmath>
#include "include/HierarchicalPathORAM.h"

HierarchicalPathORAM::HierarchicalPathORAM() {
    max_hierarchy = 20;
    hierarchy = 0;

    hier_PathORAM = new PathORAM*[max_hierarchy];
    data_size = new uint64_t[max_hierarchy];
    utilization = new double[max_hierarchy];
    block_size = new int[max_hierarchy];
    block_num_per_bucket = new int[max_hierarchy];
    bucket_count = new uint64_t[max_hierarchy];
    block_count = new uint64_t[max_hierarchy];
    leaf_count = new uint64_t[max_hierarchy];
    level_count = new int[max_hierarchy];

    position_map_scale_factor = new int[max_hierarchy];
}

HierarchicalPathORAM::~HierarchicalPathORAM() { }

/*
    ds_s: data size
    util: utilization
    HOram_bl_s: Hierarchical Path ORAM block size
    HOram_bn_p: Hierarchical Path ORAM block num per bucket
    maxPosMap_size: the specified on-chip storage
    st_s: stash size
*/
int HierarchicalPathORAM::configParameters(uint64_t ds_s, double *util, int *HOram_bl_s, int *HOram_bn_p, uint32_t maxPosMap_size, int st_s, bool isDebug) {
    assert(!hierarchy);			// should not be called twice

    data_size[0] = ds_s;
    assert(ds_s > maxPosMap_size);	

    int i = 0;
    uint64_t real_block_count = 0;
    final_position_map_size = data_size[0];
    while (final_position_map_size > (uint64_t)maxPosMap_size && i < max_hierarchy)	{	// iterate until fit into the specified on-chip storage
        hier_PathORAM[i] = NULL;
        block_size[i] = HOram_bl_s[i];
        block_num_per_bucket[i] = HOram_bn_p[i];
        utilization[i] = util[i];
        real_block_count = ceil(data_size[i] / block_size[i]);
        //  actual oram size * utilization = data size
        block_count[i] = ceil(data_size[i] / utilization[i] / block_size[i]);
        bucket_count[i] = block_count[i] / block_num_per_bucket[i];

        level_count[i] = ceil(log2(bucket_count[i] + 1));
        bucket_count[i] = (1 << level_count[i]) - 1;
        block_count[i] = bucket_count[i] * block_num_per_bucket[i];
        leaf_count[i] = (bucket_count[i] + 1) / 2;


        position_map_scale_factor[i + 1] = HOram_bl_s[i + 1] * 8 / level_count[i];   

        int log_posmap_scale_factor = log2(position_map_scale_factor[i + 1]);
        position_map_scale_factor[i + 1] = 1 << log_posmap_scale_factor;

        data_size[i + 1] = (real_block_count + position_map_scale_factor[i + 1] - 1) / position_map_scale_factor[i + 1] * HOram_bl_s[i + 1];
        final_position_map_size = real_block_count * level_count[i] / 8; 
        i++;	
    }

    hierarchy = i;		
    address = new int64_t[hierarchy];	


    debug = isDebug;
    max_stash_size = st_s;
    for (int i = 0; i < hierarchy; i++) {
        cout << "data size[" << i << "]: " << data_size[i] << endl;
        cout << "----------hier " << i << "----------- : " << endl;
        hier_PathORAM[i] = new PathORAM;
        hier_PathORAM[i]->configParameters(data_size[i], data_size[i] / utilization[i], block_size[i], block_num_per_bucket[i], max_stash_size, debug);
    }

    for (int i = 0; i < hierarchy; i++) {
        cout << "hier_PathORAM " << i << "'s block count: ";
        cout << hier_PathORAM[i]->getBlockCount() << endl;
    }

    cout << "Data size is " << data_size[0] / 1024.0 / 1024 << " MB, ORAM size is " << data_size[0] / utilization[0] / 1024.0 / 1024 << " MB." << endl;
    for (int i = 0; i < hierarchy; i++)
        cout << "ORAM " << i << " has " << level_count[i] << " levels." << endl;
    cout << "Total hierarchy is " << hierarchy << ", on-chip position map's size is " << final_position_map_size << " Bytes" << endl;

    return 1;
}

void HierarchicalPathORAM::initialize() {
    for (int i = 0; i < hierarchy; i++) {
        hier_PathORAM[i]->initialize();
        cout << "block_count[i]: " << block_count[i] << endl;
        cout << "hier_PathORAM[i]->getBlockCount(): " << hier_PathORAM[i]->getBlockCount() << endl;
        assert(block_count[i] == hier_PathORAM[i]->getBlockCount());
        assert(bucket_count[i] == hier_PathORAM[i]->getBucketCount());
        assert(level_count[i] == hier_PathORAM[i]->getLevelCount());
    }
    access_count = 0;
    dummy_access_count = 0;
}

void HierarchicalPathORAM::setDefaultLatencyParas(int h_d, int h_t_m, int r, int w_b)
{
    for (int i = hierarchy - 1; i >= 0; i--)
        hier_PathORAM[i]->setDefaultLatencyParas(h_d, h_t_m, r, w_b);
}

int64_t HierarchicalPathORAM::getRealBlockCountForHierORAM() {
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PathORAM[i]->getRealBlockCount();
    return tmp;
}



int64_t HierarchicalPathORAM::getStashHitForHierORAM() {
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PathORAM[i]->getStashHit();
    return tmp;
}

int64_t HierarchicalPathORAM::getStashMissForHierORAM() {
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PathORAM[i]->getStashMiss();
    return tmp;
}

int HierarchicalPathORAM::getHierarchy() { return hierarchy; }
int HierarchicalPathORAM::getBlockSize(int index) { return block_size[index]; }
int HierarchicalPathORAM::getBlockNumPerBucket(int index) { return block_num_per_bucket[index]; }
int64_t HierarchicalPathORAM::getBlockCount(int index) { return block_count[index]; }
int64_t HierarchicalPathORAM::getBucketCount(int index) { return bucket_count[index]; }
int64_t HierarchicalPathORAM::getPosMapScaleFactor(int index) { return position_map_scale_factor[index]; }
int64_t HierarchicalPathORAM::getLeafCount(int index) { return leaf_count[index]; }
int HierarchicalPathORAM::getLevelCount(int index) { return level_count[index]; }
int HierarchicalPathORAM::getRealBlockCountOfDataORAM() { return hier_PathORAM[0]->getRealBlockCount(); }

void HierarchicalPathORAM::resetMetricForHierORAM() {
    for (int i = hierarchy - 1; i >= 0; i--)
        hier_PathORAM[i]->resetMetric();
}

int64_t HierarchicalPathORAM::getAccessCount() {
    int64_t access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        access_count += hier_PathORAM[i]->getAccessCount();
    return access_count;
}

int64_t HierarchicalPathORAM::getDummyAccessCount() {
    int64_t dummy_access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        dummy_access_count += hier_PathORAM[i]->getDummyAccessCount();
        cout << "hier " << i << "dummy count: " << hier_PathORAM[i]->getDummyAccessCount() << endl;
    }
    return dummy_access_count;
}

int64_t HierarchicalPathORAM::getMemoryAccessCount() {
    int64_t memory_access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        memory_access_count += hier_PathORAM[i]->getMemoryAccessCount();
    return memory_access_count;
}

int64_t HierarchicalPathORAM::getActualAccessCount() {
    int64_t actual_access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        actual_access_count += hier_PathORAM[i]->getActualAccessCount();
    return actual_access_count;
}

int64_t HierarchicalPathORAM::getAccessCountOfDataORAM() {
	return hier_PathORAM[0]->getAccessCount();
}

int64_t HierarchicalPathORAM::getDummyAccessCountOfDataORAM() {
	return hier_PathORAM[0]->getDummyAccessCount();
}

int64_t HierarchicalPathORAM::getActualAccessCountOfDataORAM() {
	return hier_PathORAM[0]->getActualAccessCount();
}

int64_t HierarchicalPathORAM::getMemoryAccessCountOfDataORAM() {
	return hier_PathORAM[0]->getMemoryAccessCount();
}

int64_t HierarchicalPathORAM::getRA_MemoryAccessCount()
{
	int64_t RA_memory_access_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--) {
		RA_memory_access_count += hier_PathORAM[i]->getRA_MemoryAccessCount();
	}
	return RA_memory_access_count;
}

int64_t HierarchicalPathORAM::getDA_MemoryAccessCount()
{
	int64_t DA_memory_access_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--) {
		DA_memory_access_count += hier_PathORAM[i]->getDA_MemoryAccessCount();
	}
	return DA_memory_access_count;
}

int64_t HierarchicalPathORAM::getRA_StashHitCount()
{
	int64_t RA_stash_hit_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--) {
		RA_stash_hit_count += hier_PathORAM[i]->getRA_StashHit();
	}
	return RA_stash_hit_count;
}

int64_t HierarchicalPathORAM::getDA_StashHitCount()
{
	int64_t DA_stash_hit_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--) {
		DA_stash_hit_count += hier_PathORAM[i]->getDA_StashHit();
	}
	return DA_stash_hit_count;
}

int64_t HierarchicalPathORAM::getRA_MemoryAccessCountOfDataORAM()
{
	return hier_PathORAM[0]->getRA_MemoryAccessCount();
}

int64_t HierarchicalPathORAM::getDA_MemoryAccessCountOfDataORAM()
{
	return hier_PathORAM[0]->getDA_MemoryAccessCount();
}

int64_t HierarchicalPathORAM::getRA_StashHitCountOfDataORAM()
{
	return hier_PathORAM[0]->getRA_StashHit();
}

int64_t HierarchicalPathORAM::getDA_StashHitCountOfDataORAM()
{
	return hier_PathORAM[0]->getDA_StashHit();
}




int64_t HierarchicalPathORAM::getRA_PathReadCount() {
	int64_t RA_path_read_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		RA_path_read_count += hier_PathORAM[i]->getRA_PathReadCount();
	return RA_path_read_count;
}
int64_t HierarchicalPathORAM::getDA_PathReadCount() {
	int64_t DA_path_read_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		DA_path_read_count += hier_PathORAM[i]->getDA_PathReadCount();
	return DA_path_read_count;
}
int64_t HierarchicalPathORAM::getRA_PathWriteCount() {
	int64_t RA_path_write_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		RA_path_write_count += hier_PathORAM[i]->getRA_PathWriteCount();
	return RA_path_write_count;
}
int64_t HierarchicalPathORAM::getDA_PathWriteCount() {
	int64_t DA_path_write_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		DA_path_write_count += hier_PathORAM[i]->getDA_PathWriteCount();
	return DA_path_write_count;
}


int64_t HierarchicalPathORAM::getRA_RealBlockReadCount() {
	int64_t RA_real_block_read_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		RA_real_block_read_count += hier_PathORAM[i]->getRA_RealBlockReadCount();
	return RA_real_block_read_count;
}
int64_t HierarchicalPathORAM::getDA_RealBlockReadCount() {
	int64_t DA_real_block_read_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		DA_real_block_read_count += hier_PathORAM[i]->getDA_RealBlockReadCount();
	return DA_real_block_read_count;
}
int64_t HierarchicalPathORAM::getRA_RealBlockWriteCount() {
	int64_t RA_real_block_write_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		RA_real_block_write_count += hier_PathORAM[i]->getRA_RealBlockWriteCount();
	return RA_real_block_write_count;
}
int64_t HierarchicalPathORAM::getDA_RealBlockWriteCount() {
	int64_t DA_real_block_write_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		DA_real_block_write_count += hier_PathORAM[i]->getDA_RealBlockWriteCount();
	return DA_real_block_write_count;
}

int64_t HierarchicalPathORAM::getRA_DummyBlockReadCount() {
	int64_t RA_dummy_block_read_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		RA_dummy_block_read_count += hier_PathORAM[i]->getRA_DummyBlockReadCount();
	return RA_dummy_block_read_count;
}
int64_t HierarchicalPathORAM::getDA_DummyBlockReadCount() {
	int64_t DA_dummy_block_read_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		DA_dummy_block_read_count += hier_PathORAM[i]->getDA_DummyBlockReadCount();
	return DA_dummy_block_read_count;
}
int64_t HierarchicalPathORAM::getRA_DummyBlockWriteCount() {
	int64_t RA_dummy_block_write_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		RA_dummy_block_write_count += hier_PathORAM[i]->getRA_DummyBlockWriteCount();
	return RA_dummy_block_write_count;
}
int64_t HierarchicalPathORAM::getDA_DummyBlockWriteCount() {
	int64_t DA_dummy_block_write_count = 0;
	for (int i = hierarchy - 1; i >= 0; i--)
		DA_dummy_block_write_count += hier_PathORAM[i]->getDA_DummyBlockWriteCount();
	return DA_dummy_block_write_count;
}

uint64_t HierarchicalPathORAM::getHitLatency()
{
    uint64_t hit_latency = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        cout << "hit latency-" << i << ": " << hier_PathORAM[i]->getHitLatency() << endl;
        hit_latency += hier_PathORAM[i]->getHitLatency();
    }
    return hit_latency;
}

uint64_t HierarchicalPathORAM::getReadyLatency()
{
    uint64_t ready_latency = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        cout << "ready latency-" << i << ": " << hier_PathORAM[i]->getReadyLatency() << endl;
        ready_latency += hier_PathORAM[i]->getReadyLatency();
    }
    return ready_latency;
}

int64_t HierarchicalPathORAM::getAvgHitLatency()
{
    uint64_t sum_hit_latency = getHitLatency();
    uint64_t sum_access_count = getAccessCount();
    return ceil(sum_hit_latency *1.0 / sum_access_count);
}

int64_t HierarchicalPathORAM::getAvgReadyLatency()
{
    uint64_t sum_ready_latency = getReadyLatency();
    uint64_t sum_access_count = getAccessCount();
    return ceil(sum_ready_latency * 1.0 / sum_access_count);
}


int64_t HierarchicalPathORAM::getPathAllocateRightCountOfDataORAM()
{
    return int64_t(111);
}

int64_t HierarchicalPathORAM::getPathAllocateWrongCountOfDataORAM()
{
    return int64_t(111);
}


void HierarchicalPathORAM::setDebug(bool debug) {
    for (int i = hierarchy - 1; i >= 0; i--)
        hier_PathORAM[i]->setDebug(debug);
}

double HierarchicalPathORAM::feedbackTime()
{
	double cost = 0.0;
	for (int i = hierarchy - 1; i >= 0; i--)
		cost += hier_PathORAM[i]->feedbackTime();
	return cost;
}

void HierarchicalPathORAM::displayPosMapOfDataORAM() {
    int64_t *posMap = hier_PathORAM[0]->getPositionMap();
    for (int64_t i = 0; i < hier_PathORAM[0]->getRealBlockCount(); i++)
        cout << posMap[i] << " ";
    cout << endl;
}

void HierarchicalPathORAM::generateAddress(int64_t addr) {
    address[0] = addr;
    if(debug)
        cout << "address[0]" << address[0] << endl;
    for (int i = 1; i < hierarchy; i++) {
        if(debug)
            cout << "position_map_scale_factor[" << i << "]: " << position_map_scale_factor[i] << endl;
        address[i] = address[i - 1] / position_map_scale_factor[i];
        if(debug)
            cout << "address[" << i << "]: " << address[i] << endl;
    }
}

uint64_t HierarchicalPathORAM::access(int64_t id, short operation, int64_t data) {
    access_count++;

    uint64_t IO_traffic = 0;
    if (id < 0) {
        //	dummy_access_count += hierarchy;
        for (int i = hierarchy - 1; i >= 0; i--) {
           	//IO_traffic += hier_PathORAM[i]->access(hier_PathORAM[i]->getRealBlockCount(), PathORAM::dummy, -1);
            IO_traffic += hier_PathORAM[i]->backgroundEviction();
        }
        return IO_traffic;
    }
    else {
        for (int i = hierarchy - 1; i >= 0; i--)
            assert(hier_PathORAM[i] && !hier_PathORAM[i]->stash.isFull());		// check stash not full
    }

    //	cout << "-----------------------------------------------------" << endl;
    //	cout << "Generate Address for block " << id << " ..." << endl;
    generateAddress(id);

    if(debug) {
        for (int i = 0; i < hierarchy; i++) {
            cout << "hier_PathORAM " << i << "'s block count: ";
            cout << hier_PathORAM[i]->getBlockCount() << endl;
        }
    }

    for (int i = hierarchy - 1; i > 0; i--) {
        if(debug)
            cout << "Begin hier_PathORAM " << i << " access...--- " << address[i] << endl;
        IO_traffic += hier_PathORAM[i]->access(address[i], PathORAM::write, -1);
        if(debug)
            cout << "Finish hier_PathORAM " << i << " access..." << endl;
    }
    if(debug)
        cout << "Begin hier_PathORAM " << 0 << " access...--- " << address[0] << endl;
    IO_traffic += hier_PathORAM[0]->access(address[0], operation, data);
    if(debug) {
        cout << "Finish hier_PathORAM " << 0 << " access..." << endl;
        cout << "-----------------------------------------------------" << endl;
    }

    return IO_traffic;
}

bool HierarchicalPathORAM::isLocalcacheFull() {
    for (int i = hierarchy - 1; i >= 0; i--) {
        //	cout << "current stash size: " << hier_PathORAM[i]->stash.getCurrentStashSize() << endl;
        if (hier_PathORAM[i]->stash.isAlmostFull())
            return true;
    }
    return false;
}

uint64_t HierarchicalPathORAM::backgroundEviction() {
    //	access_count++;
    uint64_t traffic = 0;
    while (isLocalcacheFull()) {
        traffic += access(-1, PathORAM::dummy, -1);
    }
    return traffic;
}
