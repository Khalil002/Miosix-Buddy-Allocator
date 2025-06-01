#include "buddy_allocator.h"
#include <stdexcept>
#include <iostream>
#include <sstream>

#define INVALID_UINT 0xFFFFFFFF

using namespace std;

/**
 * Buddy Allocator Implementation
 * This class implements a buddy memory allocator.
 * It manages a memory pool and allows allocation, deallocation, and reallocation of memory blocks.
 */

 /**
  * \brief Constructor for the Buddy Allocator class.
  * \param memBase Pointer to the base address of the memory pool.
  * \param memSize Size of the memory pool in bytes.
  * \throws invalid_argument if the memory size is smaller than the minimum block size.
  */
Buddy::Buddy(unsigned int *memBase, unsigned int memSize)
{
    this->memBase = memBase; // Base address of the memory pool
    this->memSize = memSize; // Size of the memory pool in bytes
    minBlockExp = 10; // Minimum block exp (1 << 10 = 1024 bytes = 1 KB)
    minBlockSize = 1 << minBlockExp; // 2^minBlockExp bytes

    // Check if the memory size is smaller than the minimum block size
    if (memSize < minBlockSize) {
        throw invalid_argument("Memory size is smaller than minimum block size.");
    }

    // Align the base address to the minimum block size (1 to 1023 bytes will be padded if not alligned)
    unsigned int memBaseValue = reinterpret_cast<unsigned int>(memBase);
    if(memBaseValue % minBlockSize != 0) {
        offset = minBlockSize - memBaseValue % minBlockSize; // Offset to align the base address
    }else{
        offset = 0; // No offset needed if already aligned
    }
    unsigned int alignedBaseValue = memBaseValue + offset;
    alignedBase = reinterpret_cast<unsigned int*>(alignedBaseValue);
    alignedSize = memBaseValue + memSize - alignedBaseValue; // Size of the aligned memory pool

    maxBlockExp = ceiling_log2(alignedSize); // Maximum block exponent
    maxBlockSize = 1 << maxBlockExp; // 2^maxBlockExp bytes
    root = new Node();

    // If the aligned size is less than the maximum block size, allocate an unusable block
    // to make sure the tree does not cover memory locations outside the memory pool
    if(alignedSize < maxBlockSize) {
        unsigned int unusableSize = maxBlockSize-alignedSize;
        unsigned int unusableExp = ceiling_log2(unusableSize);
        allocateUnusableBlock(root, unusableExp, maxBlockExp);
    }
}

/**
 * \brief Allocate an unusable block in the rightmost side of the tree.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetExp The exponent of the block size to allocate (2^targetExp).
 * \param depthExp The current depth exponent in the buddy tree.
 */
void Buddy::allocateUnusableBlock(Node *node, unsigned int targetExp, unsigned int depthExp){
    if(depthExp == targetExp || depthExp == minBlockExp){
        node->occupied = true; 
        node->unusable = true; 
        return;
    }

    node->right = new Node();
    node->right->parent = node;
    return allocateUnusableBlock(node->right, targetExp, depthExp - 1);
}

/**
 * \brief Destructor for the Buddy Allocator class.
 * It destroys the buddy tree
 */
Buddy::~Buddy()
{
    destroyTree(root);
}

/**
 * \brief Allocates a memory block that covers the requested size.
 * \param size The size of the memory to allocate in bytes.
 * \return A pair containing a pointer to the allocated memory and its size.
 */
pair<unsigned int *, unsigned int>Buddy::allocate(unsigned int size){
    if (size < minBlockSize || size > maxBlockSize) {
        throw invalid_argument("Invalid allocation size.");
    }

    unsigned int blockExp = ceiling_log2(size);
    unsigned int blockSize = 1 << blockExp;
    unsigned int *ptr = allocateRecursive(root, blockExp, maxBlockExp, alignedBase);

    return make_pair(ptr, blockSize); // Return the pointer to the allocated memory and its size
}

unsigned int *Buddy::allocateRecursive(Node *node, unsigned int targetExp, unsigned int depthExp, unsigned int *memPtr){
    if (node->occupied || node->unusable) return nullptr;

    if(depthExp == targetExp){
        if(!node->left && !node->right){
            node->occupied = true;
            return memPtr;
        }else{
            return nullptr;
        }
    } 

    // Try allocating in left subtree
    unsigned int* leftMemPtr = memPtr;
    if (!node->left){
        node->left = new Node();
        node->left->parent = node;
    }
    unsigned int* leftResult = allocateRecursive(node->left, targetExp, depthExp - 1, leftMemPtr);
    if (leftResult) return leftResult;

    // If left allocation failed, try allocating in right subtree
    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int memPtrValue = reinterpret_cast<unsigned int>(memPtr);
    unsigned int* rightMemPtr = reinterpret_cast<unsigned int*>(memPtrValue + local_offset);
    if (!node->right){
        node->right = new Node();
        node->right->parent = node;
    }
    return allocateRecursive(node->right, targetExp, depthExp - 1, rightMemPtr);
}

void Buddy::deallocate(unsigned int *ptr){
    // No action if the pointer is null
    if (!ptr) return;

    // Check if the pointer is within the bounds of the memory pool
    unsigned int ptrValue = reinterpret_cast<unsigned int>(ptr);
    unsigned int alignedBaseValue = reinterpret_cast<unsigned int>(alignedBase);
    if(ptrValue < alignedBaseValue || ptrValue >= (alignedBaseValue + alignedSize)) {
        throw invalid_argument("Pointer is out of bounds of the memory pool.");
    }
    // Calculate the offset from the aligned base address
    if (ptrValue % minBlockSize != 0) {
        throw invalid_argument("Pointer is not aligned to the minimum block size.");
    }

    // Deallocate recursively
    deallocateRecursive(root, ptr, maxBlockExp, alignedBase);
}

void Buddy::deallocateRecursive(Node *node, unsigned int *targetPtr, unsigned int depthExp, unsigned int* memPtr) {
    // Base case: if the node is null or unusable, return
    if (!node || node->unusable) return;

    // Success case: if the node is occupied, we have found the target block
    if (node->occupied) {
        node->occupied = false;
        backPropagateDeallocate(node);
        return;
    }

    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int memPtrValue = reinterpret_cast<unsigned int>(memPtr);
    unsigned int* leftMemPtr = memPtr;
    unsigned int* rightMemPtr = reinterpret_cast<unsigned int*>(memPtrValue + local_offset);
    if(leftMemPtr <= targetPtr && rightMemPtr > targetPtr) {
        deallocateRecursive(node->left, targetPtr, depthExp - 1, leftMemPtr);
    }else{
        deallocateRecursive(node->right, targetPtr, depthExp - 1, rightMemPtr);
    }
}

void Buddy::backPropagateDeallocate(Node *node) {
    if (node == root) return; // Stop if we reach the root
    if (node->left || node->right) return; // If the node has children, we cannot delete it

    Node* parent = node->parent;
    if(parent->left == node) {
        parent->left = nullptr; // Remove the left child
    } else {
        parent->right = nullptr; // Remove the right child
    }
    delete node; // Delete the current node
    backPropagateDeallocate(parent); // Backpropagate to parent
}

unsigned int *Buddy::reallocate(unsigned int *ptr, unsigned int newSize){
    if (!ptr) throw invalid_argument("Pointer is null.");

    // Check if the pointer is within the bounds of the memory pool
    unsigned int ptrValue = reinterpret_cast<unsigned int>(ptr);
    unsigned int alignedBaseValue = reinterpret_cast<unsigned int>(alignedBase);
    if(ptrValue < alignedBaseValue || ptrValue >= (alignedBaseValue + alignedSize)) {
        throw invalid_argument("Pointer is out of bounds of the memory pool.");
    }
    // Calculate the offset from the aligned base address
    if (ptrValue % minBlockSize != 0) {
        throw invalid_argument("Pointer is not aligned to the minimum block size.");
    }

    // Obtain the block
    std::pair<unsigned int, Node *> block = get_block(root, ptr, maxBlockExp, alignedBase);
    unsigned int blockExp = block.first;
    Node *node = block.second;
    if (!node) {
        throw invalid_argument("Pointer does not belong to a block in the buddy tree.");
    }
    printf("Reallocating block of size 2^%u bytes at %p to size %u bytes\n", blockExp, ptr, newSize);
    printBT("", root, false, maxBlockExp, alignedBase);
    //manually deallocate the block
    node->occupied = false;
    printf("Buddy tree after deallocation:\n");
    printBT("", root, false, maxBlockExp, alignedBase);
    backPropagateDeallocate(node);
    printf("Buddy tree after deletion:\n");
    printBT("", root, false, maxBlockExp, alignedBase);
    // Allocate a new block with the requested size
    std::pair<unsigned int*, unsigned int> newBlock = allocate(newSize);
    unsigned int *newBlockPtr = newBlock.first;
    // If allocation failed, allocate the deallocated block
    if (!newBlockPtr) {
        printf("Allocation failed, trying to allocate the deallocated block of size 2^%u bytes at %p\n", blockExp, ptr);
        newBlockPtr = allocateSpecific(root, blockExp, ptr, maxBlockExp, alignedBase);
    }

    return newBlockPtr;
}

std::pair<unsigned int, Buddy::Node *> Buddy::get_block(Node *node, unsigned int *targetPtr, unsigned int depthExp, unsigned int* memPtr) {
    // Base case: if the node is null or unusable, return an invalid pair
    if (!node || node->unusable) return make_pair(INVALID_UINT ,nullptr);

    // Success case: if the node is occupied, we have found the target block
    if (node->occupied) {
        printf("Found occupied block of target pointer %p at depth %u in node %p\n", targetPtr, depthExp, memPtr);
        return make_pair(depthExp, node);
    }

    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int memPtrValue = reinterpret_cast<unsigned int>(memPtr);
    unsigned int* leftMemPtr = memPtr;
    unsigned int* rightMemPtr = reinterpret_cast<unsigned int*>(memPtrValue + local_offset);
    std::pair<unsigned int, Node *>result;
    if(leftMemPtr <= targetPtr && rightMemPtr > targetPtr) {
        printf("moving from %p to left child in %p \n", memPtr, leftMemPtr);
        result = get_block(node->left, targetPtr, depthExp - 1, leftMemPtr);
    }else{
        printf("moving from %p to right child in %p \n", memPtr, rightMemPtr);
        result = get_block(node->right, targetPtr, depthExp - 1, leftMemPtr);
    }
    return result;
}

unsigned int *Buddy::allocateSpecific(Node *node, unsigned int targetExp, unsigned int *targetPtr, unsigned int depthExp, unsigned int *memPtr) {
    if (node->occupied || node->unusable) return nullptr;

    if(depthExp == targetExp){
        if(!node->left && !node->right){
            node->occupied = true;
            return memPtr;
        }else{
            return nullptr;
        }
    } 

    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int memPtrValue = reinterpret_cast<unsigned int>(memPtr);
    unsigned int* leftMemPtr = memPtr;
    unsigned int* rightMemPtr = reinterpret_cast<unsigned int*>(memPtrValue + local_offset);
    
    if(leftMemPtr <= targetPtr && rightMemPtr > targetPtr) {
        if (!node->left){
            node->left = new Node();
            node->left->parent = node;
        }
        return allocateSpecific(node->left, targetExp, targetPtr, depthExp - 1, leftMemPtr);
    }else{
        if (!node->right){
            node->right = new Node();
            node->right->parent = node;
        }
        return allocateSpecific(node->right, targetExp, targetPtr, depthExp - 1, rightMemPtr);
    }
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

/**
 * Destroy the buddy tree recursively.
 * This function deletes all nodes in the buddy tree to free memory.
 * \param node Pointer to the current node in the buddy tree.
 */
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

