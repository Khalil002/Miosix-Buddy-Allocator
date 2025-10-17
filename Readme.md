
# Miosix-Buddy-Allocator
A C++ buddy memory allocator, built for the <a href="http://miosix.org">Miosix OS</a>

## Overview

This is a buddy memory allocator that utilizes a binary tree to represent the structure of the memory arena.
For more information on the buddy algorithm, check the <a href="https://en.wikipedia.org/wiki/Buddy_memory_allocation">wiki</a>

## Features

- Fixed minimum and maximum cell size
- Iterative and Recursive versions available
- Made for 32-bit platforms

## Usage

In Miosix OS, the buddy_allocator.cpp is utilized by process_pool.cpp internally when you call for allocation, deallocation and reallocation when compiled with the BMA flag.

If you wish to use the buddy allocator in your own code, here is an example:

Copy the files:
- buddy_allocator.cpp
- buddy_allocator.h
  
Initialization
```c
#include "buddy_allocator.h"

unsigned int* memBase = reinterpret_cast<unsigned int*>(0x20008000);
unsigned int memSize = 1<<14;
unsigned int minBlockExp = 10;
Buddy buddy = new Buddy(memBase, memSize, minBlockExp);
```

Allocation and Deallocation
```c
unsigned int size = 1025; //Size in bytes
pair<unsigned int *, unsigned int> res = buddy.allocate(size); //Returns the pointer and the size of the block allocated
unsigned int* ptr = res.first;

buddy.deallocate(ptr)
```

Allocation and Reallocation
```c
unsigned int size = 1025; //Size in bytes
pair<unsigned int *, unsigned int> res = buddy.allocate(size);
unsigned int* ptr = res.first;

unsigned int newSize = 4096; //Size in bytes
unsigned int* newPtr = buddy.reallocate(ptr, newSize);
```

