
#include <iostream>
#include <cassert>
#include <cmath>
#include <algorithm>
#include "include/HierachicalPCDORAM.h"

HierachicalPCDORAM::HierachicalPCDORAM()
{
    max_hierarchy = 20;
    max_hierarchy = 20;
    hierarchy = 0;

    hier_PCDORAM = new PCDORAM * [max_hierarchy];
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

HierachicalPCDORAM::~HierachicalPCDORAM() { }

/*
    ds_s: data size
    util: utilization
    HOram_bl_s: Hierarchical Path ORAM block size
    HOram_bn_p: Hierarchical Path ORAM block num per bucket
    maxPosMap_size: the specified on-chip storage
    st_s: stash sized
*/
int HierachicalPCDORAM::configParameters(uint64_t ds_s, const double* util, int* HOram_bl_s, int* HOram_bn_p, uint32_t maxPosMap_size, int st_s, bool isDebug)
{
    assert(!hierarchy);			// should not be called twice

    data_size[0] = ds_s;
    assert(ds_s > maxPosMap_size);		

    int i = 0;
    uint64_t real_block_count = 0;
    final_position_map_size = data_size[0];
    while (final_position_map_size > (uint64_t)maxPosMap_size && i < max_hierarchy)		// iterate until fit into the specified on-chip storage
    {
        hier_PCDORAM[i] = NULL;
        block_size[i] = HOram_bl_s[i];
        block_num_per_bucket[i] = HOram_bn_p[i];
        utilization[i] = util[i];
        real_block_count = ceil(data_size[i] / block_size[i]);
        block_count[i] = ceil(data_size[i] / utilization[i] / block_size[i]);
        bucket_count[i] = block_count[i] / block_num_per_bucket[i];

        level_count[i] = ceil(log2(bucket_count[i] + 1));
        bucket_count[i] = (1 << level_count[i]) - 1;
        block_count[i] = bucket_count[i] * block_num_per_bucket[i];
        leaf_count[i] = (bucket_count[i] + 1) / 2;

        // Calculating recursion size
        position_map_scale_factor[i + 1] = HOram_bl_s[i + 1] * 8 / level_count[i];
        // Power of two
        int log_posmap_scale_factor = log2(position_map_scale_factor[i + 1]);
        position_map_scale_factor[i + 1] = 1 << log_posmap_scale_factor;

        data_size[i + 1] = (real_block_count + position_map_scale_factor[i + 1] - 1) / position_map_scale_factor[i + 1] * HOram_bl_s[i + 1];
        final_position_map_size = real_block_count * level_count[i] / 8;
        i++;	

    hierarchy = i;		// Iteration Layers
    address = new int64_t[hierarchy];	// hierarchy addr

    debug = isDebug;
    max_stash_size = st_s;
    for (int i = 0; i < hierarchy; i++)
    {
        hier_PCDORAM[i] = new PCDORAM;
        hier_PCDORAM[i]->configParameters(data_size[i], data_size[i] / utilization[i], block_size[i], block_num_per_bucket[i], max_stash_size, debug);
    }

    for (int i = 0; i < hierarchy; i++)
    {
        cout << "hier_PCDORAM " << i << "'s block count: ";
        cout << hier_PCDORAM[i]->getBlockCount() << endl;
    }

    cout << "Data size is " << data_size[0] / 1024.0 / 1024 << " MB, ORAM size is " << data_size[0] / utilization[0] / 1024.0 / 1024 << " MB." << endl;
    for (int i = 0; i < hierarchy; i++)
        cout << "ORAM " << i << " has " << level_count[i] << " levels." << endl;
    cout << "Total hierarchy is " << hierarchy << ", on-chip position map's size is " << final_position_map_size << " Bytes" << endl;

    return 1;
}

void HierachicalPCDORAM::initialize()
{
    for (int i = 0; i < hierarchy; i++)
    {
        hier_PCDORAM[i]->initialize();
        cout << "block_count[i]: " << block_count[i] << endl;
        cout << "hier_PCDORAM[i]->getBlockCount(): " << hier_PCDORAM[i]->getBlockCount() << endl;
        assert(block_count[i] == hier_PCDORAM[i]->getBlockCount());
        assert(bucket_count[i] == hier_PCDORAM[i]->getBucketCount());
        assert(level_count[i] == hier_PCDORAM[i]->getLevelCount());
    }
    access_count = 0;
    dummy_access_count = 0;
}

void HierachicalPCDORAM::setDefaultLatencyParas(int h_d, int h_t_m, int r, int w_b)
{
    for (int i = hierarchy - 1; i >= 0; i--)
        hier_PCDORAM[i]->setDefaultLatencyParas(h_d, h_t_m, r, w_b);
}

int64_t HierachicalPCDORAM::getRealBlockCountForHierORAM()
{
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PCDORAM[i]->getRealBlockCount();
    return tmp;
}

void HierachicalPCDORAM::resetMetricForHierORAM()
{
    for (int i = hierarchy - 1; i >= 0; i--)
        hier_PCDORAM[i]->resetMetric();
}

void HierachicalPCDORAM::setDebug(bool debug)
{
    for (int i = hierarchy - 1; i >= 0; i--)
        hier_PCDORAM[i]->setDebug(debug);
}

double HierachicalPCDORAM::feedbackTime()
{
    double cost = 0.0;
    for (int i = hierarchy - 1; i >= 0; i--)
        cost += hier_PCDORAM[i]->feedbackTime();
    return cost;
}


int64_t HierachicalPCDORAM::getStashHitForHierORAM()
{
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PCDORAM[i]->getStashHit();
    return tmp;
}

int64_t HierachicalPCDORAM::getStashMissForHierORAM()
{
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PCDORAM[i]->getStashMiss();
    return tmp;
}

int HierachicalPCDORAM::getHierarchy() { return hierarchy; }
int HierachicalPCDORAM::getBlockSize(int index) { return block_size[index]; }
int HierachicalPCDORAM::getBlockNumPerBucket(int index) { return block_num_per_bucket[index]; }
int64_t HierachicalPCDORAM::getBlockCount(int index) { return block_count[index]; }
int64_t HierachicalPCDORAM::getBucketCount(int index) { return bucket_count[index]; }
int64_t HierachicalPCDORAM::getPosMapScaleFactor(int index) { return position_map_scale_factor[index]; }
int64_t HierachicalPCDORAM::getLeafCount(int index) { return leaf_count[index]; }
int HierachicalPCDORAM::getLevelCount(int index) { return level_count[index]; }

int HierachicalPCDORAM::getRealBlockCountOfDataORAM() { return hier_PCDORAM[0]->getRealBlockCount(); }


int64_t HierachicalPCDORAM::getAccessCount()
{
    //	return access_count;
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PCDORAM[i]->getAccessCount();
    return tmp;
}
int64_t HierachicalPCDORAM::getDummyAccessCount()
{
    int64_t tmp = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        tmp += hier_PCDORAM[i]->getDummyAccessCount();
    return tmp;
    //	return dummy_access_count;
}
int64_t HierachicalPCDORAM::getMemoryAccessCount()
{
    int64_t memory_access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        memory_access_count += hier_PCDORAM[i]->getMemoryAccessCount();
    return memory_access_count;
}
int64_t HierachicalPCDORAM::getActualAccessCount()
{
    int64_t actual_access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        actual_access_count += hier_PCDORAM[i]->getActualAccessCount();
    return actual_access_count;
}
int64_t HierachicalPCDORAM::getAccessCountOfDataORAM() {
    return hier_PCDORAM[0]->getAccessCount();
}
int64_t HierachicalPCDORAM::getDummyAccessCountOfDataORAM() {
    return hier_PCDORAM[0]->getDummyAccessCount();
}
int64_t HierachicalPCDORAM::getMemoryAccessCountOfDataORAM() {
    return hier_PCDORAM[0]->getMemoryAccessCount();
}
int64_t HierachicalPCDORAM::getActualAccessCountOfDataORAM() {
    return hier_PCDORAM[0]->getActualAccessCount();
}

int64_t HierachicalPCDORAM::getRA_MemoryAccessCount()
{
    int64_t RA_memory_access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        RA_memory_access_count += hier_PCDORAM[i]->getRA_MemoryAccessCount();
    }
    return RA_memory_access_count;
}

int64_t HierachicalPCDORAM::getDA_MemoryAccessCount()
{
    int64_t DA_memory_access_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        DA_memory_access_count += hier_PCDORAM[i]->getDA_MemoryAccessCount();
    }
    return DA_memory_access_count;
}

int64_t HierachicalPCDORAM::getRA_StashHitCount()
{
    int64_t RA_stash_hit_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        RA_stash_hit_count += hier_PCDORAM[i]->getRA_StashHit();
    }
    return RA_stash_hit_count;
}

int64_t HierachicalPCDORAM::getDA_StashHitCount()
{
    int64_t DA_stash_hit_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        DA_stash_hit_count += hier_PCDORAM[i]->getDA_StashHit();
    }
    return DA_stash_hit_count;
}

int64_t HierachicalPCDORAM::getRA_MemoryAccessCountOfDataORAM()
{
    return hier_PCDORAM[0]->getRA_MemoryAccessCount();
}

int64_t HierachicalPCDORAM::getDA_MemoryAccessCountOfDataORAM()
{
    return hier_PCDORAM[0]->getDA_MemoryAccessCount();
}

int64_t HierachicalPCDORAM::getRA_StashHitCountOfDataORAM()
{
    return hier_PCDORAM[0]->getRA_StashHit();
}

int64_t HierachicalPCDORAM::getDA_StashHitCountOfDataORAM()
{
    return hier_PCDORAM[0]->getDA_StashHit();
}



int64_t HierachicalPCDORAM::getRA_PathReadCount() {
    int64_t RA_path_read_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        RA_path_read_count += hier_PCDORAM[i]->getRA_PathReadCount();
    return RA_path_read_count;
}
int64_t HierachicalPCDORAM::getDA_PathReadCount() {
    int64_t DA_path_read_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        cout << i << ":-:-: " << hier_PCDORAM[i]->getDA_PathReadCount() << endl;
        DA_path_read_count += hier_PCDORAM[i]->getDA_PathReadCount();
    }
    return DA_path_read_count;
}
int64_t HierachicalPCDORAM::getRA_PathWriteCount() {
    int64_t RA_path_write_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        RA_path_write_count += hier_PCDORAM[i]->getRA_PathWriteCount();
    return RA_path_write_count;
}
int64_t HierachicalPCDORAM::getDA_PathWriteCount() {
    int64_t DA_path_write_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        DA_path_write_count += hier_PCDORAM[i]->getDA_PathWriteCount();
    return DA_path_write_count;
}


int64_t HierachicalPCDORAM::getRA_RealBlockReadCount() {
    int64_t RA_real_block_read_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        RA_real_block_read_count += hier_PCDORAM[i]->getRA_RealBlockReadCount();
    return RA_real_block_read_count;
}
int64_t HierachicalPCDORAM::getDA_RealBlockReadCount() {
    int64_t DA_real_block_read_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        DA_real_block_read_count += hier_PCDORAM[i]->getDA_RealBlockReadCount();
    return DA_real_block_read_count;
}
int64_t HierachicalPCDORAM::getRA_RealBlockWriteCount() {
    int64_t RA_real_block_write_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        RA_real_block_write_count += hier_PCDORAM[i]->getRA_RealBlockWriteCount();
    return RA_real_block_write_count;
}
int64_t HierachicalPCDORAM::getDA_RealBlockWriteCount() {
    int64_t DA_real_block_write_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        DA_real_block_write_count += hier_PCDORAM[i]->getDA_RealBlockWriteCount();
    return DA_real_block_write_count;
}

int64_t HierachicalPCDORAM::getRA_DummyBlockReadCount() {
    int64_t RA_dummy_block_read_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        RA_dummy_block_read_count += hier_PCDORAM[i]->getRA_DummyBlockReadCount();
    return RA_dummy_block_read_count;
}
int64_t HierachicalPCDORAM::getDA_DummyBlockReadCount() {
    int64_t DA_dummy_block_read_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        DA_dummy_block_read_count += hier_PCDORAM[i]->getDA_DummyBlockReadCount();
    return DA_dummy_block_read_count;
}
int64_t HierachicalPCDORAM::getRA_DummyBlockWriteCount() {
    int64_t RA_dummy_block_write_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        RA_dummy_block_write_count += hier_PCDORAM[i]->getRA_DummyBlockWriteCount();
    return RA_dummy_block_write_count;
}
int64_t HierachicalPCDORAM::getDA_DummyBlockWriteCount() {
    int64_t DA_dummy_block_write_count = 0;
    for (int i = hierarchy - 1; i >= 0; i--)
        DA_dummy_block_write_count += hier_PCDORAM[i]->getDA_DummyBlockWriteCount();
    return DA_dummy_block_write_count;
}

uint64_t HierachicalPCDORAM::getHitLatency()
{
    uint64_t hit_latency = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        cout << "hit latency-" << i << ": " << hier_PCDORAM[i]->getHitLatency() << endl;
        hit_latency += hier_PCDORAM[i]->getHitLatency();
    }
    return hit_latency;
}

uint64_t HierachicalPCDORAM::getReadyLatency()
{
    uint64_t ready_latency = 0;
    for (int i = hierarchy - 1; i >= 0; i--) {
        cout << "ready latency-" << i << ": " << hier_PCDORAM[i]->getReadyLatency() << endl;
        ready_latency += hier_PCDORAM[i]->getReadyLatency();
    }
    return ready_latency;
}

int64_t HierachicalPCDORAM::getAvgHitLatency()
{
    uint64_t sum_hit_latency = getHitLatency();
    uint64_t sum_access_count = getAccessCount();
    return ceil(sum_hit_latency * 1.0 / sum_access_count);
}

int64_t HierachicalPCDORAM::getAvgReadyLatency()
{
    uint64_t sum_ready_latency = getReadyLatency();
    uint64_t sum_access_count = getAccessCount();
    return ceil(sum_ready_latency * 1.0 / sum_access_count);
}

int64_t HierachicalPCDORAM::getPathAllocateRightCountOfDataORAM()
{
    return hier_PCDORAM[0]->getPathAllocateRightCount();
}

int64_t HierachicalPCDORAM::getPathAllocateWrongCountOfDataORAM()
{
    return hier_PCDORAM[0]->getPathAllocateWrongCount();
}

void HierachicalPCDORAM::displayPosMapOfDataORAM()
{
    int64_t* posMap = hier_PCDORAM[0]->getPositionMap();
    for (int64_t i = 0; i < hier_PCDORAM[0]->getRealBlockCount(); i++)
        cout << i << "-" << posMap[i] << " ";
    cout << endl;
}

void HierachicalPCDORAM::generateAddress(int64_t addr)
{
    address[0] = addr;
    for (int i = 1; i < hierarchy; i++)
    {
        //	cout << "position_map_scale_factor[" << i << "]: " << position_map_scale_factor[i] << endl;
        address[i] = address[i - 1] / position_map_scale_factor[i];
        //	cout << "address[" << i << "]: " << address[i] << endl;
        assert(address[i] >= 0);
    }
}

uint64_t HierachicalPCDORAM::access(int64_t id, short operation, int64_t data)
{
    access_count++;

    uint64_t IO_traffic = 0;
    if (id < 0)
    {
        //	dummy_access_count += hierarchy;
        for (int i = hierarchy - 1; i >= 0; i--)
        {
            //IO_traffic += hier_PCDORAM[i]->access(hier_PCDORAM[i]->getRealBlockCount(), PCDORAM::dummy, -1);
            IO_traffic += hier_PCDORAM[i]->backgroundEviction();
        }

        return IO_traffic;
    }
    else
    {
        for (int i = hierarchy - 1; i >= 0; i--)
            assert(hier_PCDORAM[i] && !hier_PCDORAM[i]->stash.isFull());		// check stash not full
    }
    //	cout << "-----------------------------------------------------" << endl;
    //	cout << "Generate Address for block " << id << " ..." << endl;
    generateAddress(id);

    for (int i = hierarchy - 1; i > 0; i--)
    {
        //	cout << "Begin hier_PCDORAM " << i << " access..." << endl;
        IO_traffic += hier_PCDORAM[i]->access(address[i], PCDORAM::write, -1);
        //	cout << "Finish hier_PCDORAM " << i << " access..." << endl;
    }
    //	cout << "Begin hier_PCDORAM " << 0 << " access..." << endl;
    IO_traffic += hier_PCDORAM[0]->access(address[0], operation, data);
    //	cout << "Finish hier_PCDORAM " << 0 << " access..." << endl;
    //	cout << "-----------------------------------------------------" << endl;
    return IO_traffic;
}

bool HierachicalPCDORAM::isLocalcacheFull()
{
    for (int i = hierarchy - 1; i >= 0; i--)
        if (hier_PCDORAM[i]->stash.isAlmostFull())
            return true;
    return false;
}

uint64_t HierachicalPCDORAM::backgroundEviction()
{
    //	access_count++;
    uint64_t traffic = 0;
    while (isLocalcacheFull())
    {
        traffic += access(-1, PCDORAM::dummy, -1);
    }
    return traffic;
}
