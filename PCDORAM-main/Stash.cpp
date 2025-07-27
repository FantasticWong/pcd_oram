#include "include/Stash.h"

Stash::Stash() {
    max_stash_size = 1024 * 1024;		
    peak_occupancy = 0;
    last_occupancy = 0;
    limit_factor = 0.7;
}

Stash::~Stash() {}

void Stash::updatePeakAndLastOccupancy() {
    last_occupancy = local_cache.size();
    peak_occupancy = (last_occupancy > peak_occupancy) ? last_occupancy : peak_occupancy;
}

bool Stash::isFull(int margin) {
    int upper_limit = max_stash_size - margin - Z_value * L_value;
    if (local_cache.size() >= upper_limit)
        return true;
    return false;
}


bool Stash::isAlmostFull() {
    int upper_limit = max_stash_size - Z_value * L_value;
    if (local_cache.size() >= limit_factor * upper_limit)
        return true;
    return false;
}

void Stash::displayStash() {
    for (iter = local_cache.begin(); iter != local_cache.end(); iter++)
        cout << "(" << iter->id << ", " << *(iter->position) << "), ";
    cout << endl;
}

// setter func()
void Stash::setMaxStashSize(int size) { max_stash_size = size; }
void Stash::setZvalue(int z) { Z_value = z; }
void Stash::setLvalue(int l) { L_value = l; }
void Stash::setBlockSize(int bs) { block_size = bs; }

// getter func()
int Stash::getMaxStashSize() { return max_stash_size; }
int Stash::getPeakOccupancy() { return peak_occupancy; }
int Stash::getLastOccupancy() { return last_occupancy; }
int Stash::getCurrentStashSize() { return local_cache.size(); }