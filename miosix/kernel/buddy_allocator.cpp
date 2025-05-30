#include "buddy_allocator.h"
#include <stdexcept>
#include <iostream>
#define INVALID_UINT 0xFFFFFFFF
using namespace std;

Buddy::Buddy(unsigned int *memBase, unsigned int memSize)
{
    memBase = memBase; // Base address of the memory pool
    memSize = memSize; // Size of the memory pool in bytes
    minBlockExp = 10; // Minimum block exp (1 << 10 = 1024 bytes = 1 KB)
    minBlockSize = 1 << minBlockExp; // 2^minBlockExp bytes

    if (memSize < minBlockSize) {
        throw invalid_argument("Memory size is smaller than minimum block size.");
    }
    // Align the base address to the minimum block size (1 to 1023 bytes will be padded)
    unsigned int baseAddress = reinterpret_cast<unsigned int>(memBase);
    offset = minBlockSize - baseAddress % minBlockSize; // Offset to align the base address
    unsigned int alignedBaseAddress = baseAddress + offset;
    alignedBase = reinterpret_cast<unsigned int*>(alignedBaseAddress);
    alignedSize = baseAddress + memSize - alignedBaseAddress; // Size of the aligned memory pool

    maxBlockExp = ceiling_log2(alignedSize); // Maximum block exponent
    maxBlockSize = 1 << maxBlockExp; // 2^maxBlockExp bytes
    root = new Node();

    if(alignedSize < maxBlockSize) {
        unsigned int size = maxBlockSize-alignedSize;
        unsigned int blockExp = (size + minBlockSize - 1) / minBlockSize;
        allocateVirtualBlocks(root, blockExp, maxBlockExp);
    }
    printf("memory pool initialized with base address: %p, size: %u bytes\n", memBase, memSize);
    printf("Minimum block size: %u bytes, Maximum block size: %u bytes\n", minBlockSize, maxBlockSize);
    printf("Minimum block exponent: %u, Maximum block exponent: %u\n", minBlockExp, maxBlockExp);
    printf("Aligned base address: %p, Aligned size: %u bytes\n", alignedBase, alignedSize);
    printf("offset: %u bytes\n", offset);
}

void Buddy::allocateVirtualBlocks(Node *node, unsigned int blockExp, unsigned int depthExp){
    if (node == nullptr || node->occupied || node->unusable)
        return;

    if(depthExp==minBlockExp-1){
        return; // Reached the minimum block size without finding a suitable block
    }

    if(blockExp == depthExp){
        node->occupied = true; // Mark the node as occupied
        node->unusable = true; // Mark the node as unusable
        return; // Return the memory location for this block
    }

    //we allocate virtual blocks on the right most side of the tree    
    node->right = new Node();
    // Calculate new memory location for right buddy

    // Try allocating in right subtree
    return allocateVirtualBlocks(node->right, blockExp, depthExp - 1);
}

Buddy::~Buddy()
{
    destroyTree(root);
}

pair<unsigned int *, unsigned int>Buddy::allocate(unsigned int size){
    if (size == 0 || size > maxBlockSize) {
        throw invalid_argument("Invalid allocation size.");
    }

    unsigned int blockExp = ceiling_log2(size);
    if (blockExp < minBlockExp || blockExp > maxBlockExp) {
        throw invalid_argument("Requested size is out of bounds.");
    }

    unsigned int *ptr;
    if(blockExp == maxBlockExp && root->occupied == false && root->left == nullptr && root->right == nullptr) {
        printf("Allocating maximum block size: %u bytes\n", 1 << blockExp);
        root->occupied = true; // Mark the root as occupied
        ptr = alignedBase; // If the maximum block size is requested and the root is not occupied, return the aligned base address
        return make_pair(ptr, 1<<blockExp); // Return the pointer to the allocated memory and its size
    }
    ptr = allocateRecursive(root, blockExp, maxBlockExp, alignedBase);
    return make_pair(ptr, 1<<blockExp); // Return the pointer to the allocated memory and its size
}

unsigned int *Buddy::allocateRecursive(Node *node, unsigned int blockExp, unsigned int depthExp, unsigned int *memLocation){
    if (node == nullptr || node->occupied || node->unusable)
        return nullptr;

    if(depthExp==minBlockExp-1){
        return nullptr; // Reached the minimum block size without finding a suitable block
    }

    if(blockExp == depthExp && node->left == nullptr && node->right == nullptr){
        node->occupied = true; // Mark the node as occupied
        return memLocation; // Return the memory location for this block
    }

    if (node->left == nullptr)
        node->left = new Node();

    // Try allocating in left subtree
    unsigned int* leftResult = allocateRecursive(node->left, blockExp, depthExp - 1, memLocation);
    if (leftResult != nullptr)
        return leftResult;

    // Create right if needed
    if (node->right == nullptr)
        node->right = new Node();

    // Calculate new memory location for right buddy
    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int* rightMemLocation = reinterpret_cast<unsigned int*>(
        reinterpret_cast<unsigned int>(memLocation) + local_offset
    );

    // Try allocating in right subtree
    return allocateRecursive(node->right, blockExp, depthExp - 1, rightMemLocation);
}

void Buddy::deallocate(unsigned int *ptr){
    if (ptr == nullptr) return;

    // Calculate the offset from the aligned base address
    unsigned int pointerAddress = reinterpret_cast<unsigned int>(ptr);
    if (pointerAddress % minBlockSize != 0) {
        throw invalid_argument("Pointer is not aligned to the minimum block size.");
    }

    // Check if the pointer is within the bounds of the memory pool
    if( ptr < alignedBase ||
        pointerAddress >= (reinterpret_cast<unsigned int>(alignedBase) + alignedSize)) {
        throw invalid_argument("Pointer is out of bounds of the memory pool.");
    }

    // Deallocate recursively
    deallocateRecursive(root, maxBlockExp, alignedBase, ptr);
}

void Buddy::deallocateRecursive(Node *node, unsigned int depthExp, unsigned int* memLocation, unsigned int *ptr){
    if (node == nullptr || node->unusable) return;

    if (node->occupied && memLocation == ptr) {
        node->occupied = false; // Mark the node as free
        backPropagateDeallocate(node);
        return;
    }

    if (depthExp == minBlockExp - 1) return; // Reached the minimum block size without finding the block

    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int* leftMemLocation = memLocation;
    unsigned int* rightMemLocation = reinterpret_cast<unsigned int*>(
        reinterpret_cast<unsigned int>(memLocation) + local_offset);

    if(memLocation == ptr){
        deallocateRecursive(node->left, depthExp - 1, leftMemLocation, ptr);
    }else{
        deallocateRecursive(node->right, depthExp - 1, rightMemLocation, ptr);
    }
}

void Buddy::backPropagateDeallocate(Node *node) {
    if (node == nullptr) return; 
    if (node == root) return; // Stop at the root

    if(!node->occupied && !node->unusable &&
        node->left == nullptr && node->right == nullptr) {
        Node* parentPtr = node->parent;
        delete node; // Deallocate the current node
        backPropagateDeallocate(parentPtr); // Backpropagate to parent
    }
}

unsigned int *Buddy::reallocate(unsigned int *ptr, unsigned int new_size){
    if (ptr == nullptr) {
        throw invalid_argument("Pointer is null.");
    }

    // Calculate the offset from the aligned base address
    unsigned int pointerAddress = reinterpret_cast<unsigned int>(ptr);
    if (pointerAddress % minBlockSize != 0) {
        throw invalid_argument("Pointer is not aligned to the minimum block size.");
    }

    // Check if the pointer is within the bounds of the memory pool
    if( ptr < alignedBase ||
        pointerAddress >= (reinterpret_cast<unsigned int>(alignedBase) + alignedSize)) {
        throw invalid_argument("Pointer is out of bounds of the memory pool.");
    }

    // Deallocate the current block
    unsigned int blockExp = get_exp_of_block(root, maxBlockExp, alignedBase, ptr);
    deallocate(ptr);

    // Allocate a new block with the requested size
    std::pair<unsigned int*, unsigned int> result = allocate(new_size);
    unsigned int *new_ptr = result.first;

    if (new_ptr == nullptr) {
        new_ptr = allocateSpecific(root, blockExp, maxBlockExp, alignedBase, ptr);
    }
    return new_ptr;
}

unsigned int Buddy::get_exp_of_block(Node *node, unsigned int depthExp, unsigned int* memLocation, unsigned int *ptr) {

    if (node == nullptr || node->unusable) return INVALID_UINT;
    if (depthExp == minBlockExp - 1) return INVALID_UINT; // Reached the minimum block size without finding the block

    if (node->occupied && memLocation == ptr) {
        return depthExp;
    }

    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int* leftMemLocation = memLocation;
    unsigned int* rightMemLocation = reinterpret_cast<unsigned int*>(
        reinterpret_cast<unsigned int>(memLocation) + local_offset);
    
    unsigned int result;
    if(memLocation == ptr){
        result = get_exp_of_block(node->left, depthExp - 1, leftMemLocation, ptr);
    }else{
        result = get_exp_of_block(node->right, depthExp - 1, rightMemLocation, ptr);
    }
    return result;
}

unsigned int *Buddy::allocateSpecific(Node *node, unsigned int blockExp, unsigned int depthExp, unsigned int *memLocation, unsigned int *ptr) {
    if (node == nullptr || node->occupied || node->unusable)
        return nullptr;

    if(depthExp==minBlockExp-1){
        return nullptr; // Reached the minimum block size without finding a suitable block
    }

    if(blockExp == depthExp){
        node->occupied = true; // Mark the node as occupied
        return memLocation; // Return the memory location for this block
    }


    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int* leftMemLocation = memLocation;
    unsigned int* rightMemLocation = reinterpret_cast<unsigned int*>(
        reinterpret_cast<unsigned int>(memLocation) + local_offset);
    
    unsigned int *result;
    if(memLocation == ptr){
        result = allocateSpecific(node->left, blockExp, depthExp - 1, leftMemLocation, ptr);
    }else{
        result = allocateSpecific(node->right,  blockExp, depthExp - 1, rightMemLocation, ptr);
    }
    return result;
}

/*
 * Calculate the ceiling of log base 2 of a number.
 * This function returns the smallest integer e such that 2^e >= x.
 */
unsigned int Buddy::ceiling_log2(unsigned int x)
{
    if (x == 0) return 0;
    x--;
    unsigned int e = 0;
    while (x > 0) {
        x >>= 1;
        ++e;
    }
    return e;
}

void Buddy::destroyTree(Node* node)
{
    if (node == nullptr) return;
    destroyTree(node->left);
    destroyTree(node->right);
    delete node;
}

void Buddy::printBT(const std::string& prefix, const Node* node, bool isLeft, unsigned int depthExp, unsigned int* memLocation)
{
    if( node != nullptr )
    {
        std::cout << prefix;

        std::cout << (isLeft ? "├──" : "└──" );

        // print the value of the node
        if (node->unusable) {
            std::cout << depthExp << " UNUSABLE " << memLocation << std::endl;
        } else if (node->occupied) {
            std::cout << depthExp << " OCCUPIED " << memLocation << std::endl;
        } else {
            std::cout << depthExp << " FREE " << memLocation << std::endl;
        }
        
        unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
        unsigned int* leftMemLocation = memLocation;
        unsigned int* rightMemLocation = reinterpret_cast<unsigned int*>(
            reinterpret_cast<unsigned int>(memLocation) + local_offset);
        // enter the next tree level - left and right branch
        printBT( prefix + (isLeft ? "│   " : "    "), node->left, true, depthExp - 1, leftMemLocation);
        printBT( prefix + (isLeft ? "│   " : "    "), node->right, false, depthExp - 1, rightMemLocation);
    }
}

void Buddy::printBuddy()
{
    printBT("", root, false, maxBlockExp, alignedBase);  
}

