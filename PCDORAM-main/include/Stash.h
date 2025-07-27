#ifndef LOLLIRAM_STASH_H
#define LOLLIRAM_STASH_H

#include "LocalCacheLine.h"
#include <list>

class Stash
{
private:
    int max_stash_size;		
    int peak_occupancy;		
    int last_occupancy;		
    int Z_value;
    int L_value;
    int block_size;
    float limit_factor;    
public:
    list<LocalCacheLine> local_cache;
    list<LocalCacheLine>::iterator iter;

    Stash();

    void updatePeakAndLastOccupancy();
    bool isFull(int margin = 0);

    bool isAlmostFull();
    void displayStash();

    // setter func()
    void setMaxStashSize(int size);
    void setZvalue(int z);
    void setLvalue(int l);
    void setBlockSize(int bs);

    // getter func()
    int getMaxStashSize();
    int getPeakOccupancy();
    int getLastOccupancy();
    int getCurrentStashSize();

    ~Stash();
};

#endif //LOLLIRAM_STASH_H
