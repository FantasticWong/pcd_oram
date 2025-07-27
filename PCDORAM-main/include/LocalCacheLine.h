#pragma once

#include <iostream>
using namespace std;

class LocalCacheLine
{
public:
    int64_t id;				
    int64_t* position;		

	LocalCacheLine() { }
	LocalCacheLine(int64_t id, int64_t* posMap)
	{
		this->id = id;
		this->position = posMap + id;	
	}

	~LocalCacheLine() { }
};
