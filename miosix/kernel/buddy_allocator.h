/***************************************************************************
 *   Copyright (C) 2025 by Khalil El Hage Kassem                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/ 

#pragma once
#include <utility>

#ifdef TEST_ALLOC
#include <string>
#endif //TEST_ALLOC

class Buddy 
{
public:
    /**
     * Constructor.
     * \param memBase address of the start of the memory pool.
     * \param memSize size of the memory pool.
     * \param minBlockExp exponent for the minimum block size (default is 10, which means 1024 bytes).
     */
    Buddy(unsigned int *memBase, unsigned int memSize, unsigned int minBlockExp=10);
    
    /**
     * Destructor
     */
    ~Buddy();

    /**
     * \brief Allocate a block of memory of size 2^⌈log2(size)⌉.
     * \param size The size of memory to allocate in bytes.
     * \return Pointer to the allocated memory block, or nullptr if allocation fails.
     */
    std::pair<unsigned int *, unsigned int>allocate(unsigned int size);

    /**
     * \brief Free the allocated memory block.
     * \param ptr Pointer to the memory block to free.
     */
    void deallocate(unsigned int *ptr);

    /**
     * \brief Reallocate a memory block into a block of the given size (data is not copied).
     * \param ptr Pointer to the memory block.
     * \param newSize The new size of the memory block in bytes.
     * \return Pointer to the reallocated memory block, in case of failure it returns the original pointer.
     */
    unsigned int *reallocate(unsigned int *ptr, unsigned int newSize);

    #ifdef TEST_ALLOC
    /**
     * \brief Print the metadata of the buddy allocator.
     */
    void printMetadata() const;

    /**
     * \brief Print the buddy allocator tree.
     */
    void printBuddy();
    #endif //TEST_ALLOC

private:

    struct Node 
    {
        Node* left;
        Node* right;
        bool unusable;
        Node() : left(nullptr), right(nullptr), unusable(false) {}
    };

    #ifndef RECURSIVE_IMPLEMENTATION
    struct Frame {
        Node* prevNode;
        Node* node;
        unsigned int depth;
        unsigned int ptr;
        bool newNode;
        bool isLeft;
    };
    #endif //RECURSIVE_IMPLEMENTATION

    unsigned int ceiling_log2(unsigned int x);
    unsigned int floor_log2(unsigned int x);
    #ifdef RECURSIVE_IMPLEMENTATION
    void allocateUnusableBlocks(Node* node, unsigned int depth, unsigned int memPtr, unsigned int maxPtr);
    unsigned int *allocate(Node *node, unsigned int targetExp, unsigned int depthExp, unsigned int memPtrValue, bool newNode=false);
    unsigned int *allocateSpecific(Node *node, unsigned int targetExp, unsigned int targetPtrValue, unsigned int depthExp, unsigned int memPtrValue, bool newNode=false);
    unsigned int deallocate(Node *parentNode, Node *node, unsigned int targetPtr, unsigned int depthExp, unsigned int memPtr, bool isLeft);
    void destroyTree(Node* node);
    #else
    void allocateUnusableBlocksIterative();
    unsigned int *allocateIterative(unsigned int targetExp);
    unsigned int *allocateSpecificIterative(unsigned int targetExp, unsigned int targetPtrValue);
    unsigned int deallocateIterative(unsigned int ptr);
    void destroyTreeIterative(Node* node);
    #endif //RECURSIVE_IMPLEMENTATION

    #ifdef TEST_ALLOC
    void printBT(const std::string& prefix, const Node* node, bool isLeft, unsigned int depthExp, unsigned int* memLocation);
    #endif //TEST_ALLOC
    
    unsigned int *memBase;
    unsigned int memSize;
    unsigned int minBlockExp;
    unsigned int maxBlockExp;
    unsigned int minBlockSize;
    unsigned int maxBlockSize;
    unsigned int offset;
    unsigned int *alignedBase;
    unsigned int alignedSize;
    bool isRootOccupied; 
    bool isRootUnusable; 
    Node* root;
};