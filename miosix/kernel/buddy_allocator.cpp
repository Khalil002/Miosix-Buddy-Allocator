#include "buddy_allocator.h"
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <vector>
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
    alignedSize = memSize - offset; // Size of the aligned memory pool

    /* Check if the aligned size is smaller than the minimum block size.
     * This case yields an allocator with no usable blocks which even 
     * though it is valid, it is not useful so it is better to notify the user.
     */
    if (alignedSize < minBlockSize) {
        throw invalid_argument("(Memory size - alignment padding) is smaller than minimum block size.");
    }

    maxBlockExp = ceiling_log2(alignedSize); // Maximum block exponent
    maxBlockSize = 1 << maxBlockExp; // 2^maxBlockExp bytes
    root = new Node();

    // If the aligned size is less than the maximum block size, allocate an unusable block
    // to make sure the tree does not cover memory locations outside the memory pool
    if(alignedSize < maxBlockSize) {
        this->isRootUnusable = true;
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

    // Root case
    if(blockExp == maxBlockExp) {
        if(isRootOccupied || isRootUnusable) return make_pair(nullptr, 0);
        isRootOccupied = true; // Mark the root as occupied
        return make_pair(alignedBase, blockSize); // Return the aligned base address
    }

    unsigned int *ptr = allocateRecursive(root, blockExp, maxBlockExp, alignedBase);
    if (!ptr) {
        return make_pair(nullptr, 0); // If allocation failed, return nullptr
    }
    return make_pair(ptr, blockSize); // Return the pointer to the allocated memory and its size
}

/**
 * \brief Recursively allocate a memory block in the buddy tree.
 * This function traverses the buddy tree to find a suitable block for allocation.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetExp The exponent of the block size to allocate (2^targetExp).
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memPtr Pointer to the memory location that the current node manages.
 * \return Pointer to the allocated memory, or nullptr if allocation fails.
 */
unsigned int *Buddy::allocateRecursive(Node *node, unsigned int targetExp, unsigned int depthExp, unsigned int *memPtr){
    if (node->unusable) return nullptr;

    unsigned int* leftMemPtr = memPtr;
    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int memPtrValue = reinterpret_cast<unsigned int>(memPtr);
    unsigned int* rightMemPtr = reinterpret_cast<unsigned int*>(memPtrValue + local_offset);

    if (depthExp == targetExp+1){
        if (!node->left){
            node->left = new Node();
            node->left->parent = node;
            return leftMemPtr; // Allocate in the left child
        }else if(!node->right){
            node->right = new Node();
            node->right->parent = node;
            return rightMemPtr; // Allocate in the right child
        }else{
            return nullptr; // If both children exist, allocation fails
        }
    }
    
    if (!node->left){
        node->left = new Node();
        node->left->parent = node;
    }
    unsigned int* leftResult = allocateRecursive(node->left, targetExp, depthExp - 1, leftMemPtr);
    if (leftResult) return leftResult;

    if (!node->right){
        node->right = new Node();
        node->right->parent = node;
    }
    return allocateRecursive(node->right, targetExp, depthExp - 1, rightMemPtr);
}

/**
 * \brief Deallocate a memory block.
 * This function deallocates a memory block pointed to by the given pointer.
 * \param ptr Pointer to the memory block to deallocate.
 * \throws invalid_argument if the pointer is null, out of bounds, or not aligned.
 */
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
    //deallocateRecursive(root, ptr, maxBlockExp, alignedBase);
    deallocateIterative(ptr); // Use iterative deallocation for better performance
}

void Buddy::deallocateIterative(unsigned int *ptr) {
    //struct Frame { Node* node; bool isRight; unsigned int *memPtr; unsigned int depth; };
    vector<Node *> path;
    Node* node = root;
    bool found = false;
    unsigned int depthExp = maxBlockExp;
    unsigned int targetMem = reinterpret_cast<unsigned int>(ptr);
    unsigned int currentMem = reinterpret_cast<unsigned int>(alignedBase);
    
    while(true){
        path.push_back(node); // Store the current node in the path

        if(node->unusable || depthExp<minBlockExp) break;

        if(!node->left && !node->right && currentMem == targetMem) {
            found = true; // Found the target node
            break;
        }

        unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
        unsigned int leftMem = currentMem;
        unsigned int rightMem = currentMem + local_offset;

        if(leftMem <= targetMem && rightMem > targetMem) {
            if(node->right) path.erase(path.begin(), path.end() - 1);
            currentMem = leftMem; // Update current memory location
            node = node->left; // Move to the left child
        } else {
            if(!node->right) break;
            if(node->left) path.erase(path.begin(), path.end() - 1);
            currentMem = rightMem; // Update current memory location
            node = node->right; // Move to the right child
        }
        depthExp--; // Decrease the depth exponent
    }

    if(!found) return; // If the target node was not found, do nothing

    // If the target node is the root, mark it as not occupied
    if(depthExp == maxBlockExp) {
        isRootOccupied = false;
        return;
    }


    // Delete pointer to the left or right child of the parent node of the path
    Node *firstNode = path[0];
    Node *secondNode = path[1];
    if(firstNode->left == secondNode) {
        firstNode->left = nullptr; // Remove the left child
    } else {
        firstNode->right = nullptr; // Remove the right child
    }
    // Delete all nodes in the path from the second node to the end
    for(unsigned int i = 1; i < path.size(); i++) {
        Node *currentNode = path[i];
        delete currentNode; 
    }
}

/**
 * \brief Recursively deallocate a memory block in the buddy tree.
 * This function traverses the buddy tree to find the block that contains the target pointer and deallocates it.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetPtr Pointer to the target memory location to deallocate.
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memPtr Pointer to the memory location that the current node manages.
 */
void Buddy::deallocateRecursive(Node *node, unsigned int *targetPtr, unsigned int depthExp, unsigned int* memPtr) {
    // Base case: if the node is null or unusable, return
    if (!node || node->unusable) return;

    // Success case: if the node has no children and pointer matches, deallocate it
    if (!node->left && !node->right && memPtr == targetPtr) {
        // If the node is the root, we only need to mark it as not occupied
        if(depthExp == maxBlockExp) {
            isRootOccupied = false;
        }else{
            backPropagateDeallocate(node);
        }
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

/**
 * \brief Backpropagate deallocation to remove empty nodes.
 * This function removes the current node if it has no children and backpropagates to its parent.
 * \param node Pointer to the current node in the buddy tree.
 */
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

/**
 * \brief Reallocate a memory block into a block of the given size (data is not copied).
 * \param ptr Pointer to the memory block.
 * \param newSize The new size of the memory block in bytes.
 * \return Pointer to the reallocated memory block, in case of failure it returns the original pointer.
 * \throws invalid_argument if the pointer is null or out of bounds or not aligned.
 */
unsigned int *Buddy::reallocate(unsigned int *ptr, unsigned int newSize){
    if (newSize < minBlockSize || newSize > maxBlockSize) {
        throw invalid_argument("Invalid allocation size.");
    }

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
    std::pair<Node *, unsigned int> block = get_block(root, ptr, maxBlockExp, alignedBase);
    Node *node = block.first;
    unsigned int blockExp = block.second;
    if (!node) {
        throw invalid_argument("Pointer does not belong to a usable block.");
    }

    //manually deallocate the block
    if(blockExp == maxBlockExp) {
        isRootOccupied = false; // Mark the root as not occupied
    }else{
        backPropagateDeallocate(node);
    }

    // Allocate a new block with the requested size
    std::pair<unsigned int*, unsigned int> newBlock = allocate(newSize);
    unsigned int *newBlockPtr = newBlock.first;

    // If allocation failed, allocate the deallocated block
    if (!newBlockPtr) {
        newBlockPtr = allocateSpecific(root, blockExp, ptr, maxBlockExp, alignedBase);
    }

    return newBlockPtr;
}

/**
 * \brief Get the block that manages the memory pointed by the target pointer.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetPtr Pointer to the target memory location.
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memPtr Pointer to the memory location that the current node manages.
 * \return A pair containing the depth exponent and a pointer to the node containing the target block, or an invalid pair if not found.
 */
std::pair<Buddy::Node *, unsigned int> Buddy::get_block(Node *node, unsigned int *targetPtr, unsigned int depthExp, unsigned int* memPtr) {
    // Base case: if the node is null or unusable, return an invalid pair
    if (!node || node->unusable) return make_pair(nullptr, INVALID_UINT);

    // Success case: if the node is occupied, we have found the target block
    if (!node->left && !node->right && memPtr == targetPtr) {
        return make_pair(node, depthExp);
    }

    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int memPtrValue = reinterpret_cast<unsigned int>(memPtr);
    unsigned int* leftMemPtr = memPtr;
    unsigned int* rightMemPtr = reinterpret_cast<unsigned int*>(memPtrValue + local_offset);
    std::pair<Node *, unsigned int>result;
    if(leftMemPtr <= targetPtr && rightMemPtr > targetPtr) {
        result = get_block(node->left, targetPtr, depthExp - 1, leftMemPtr);
    }else{
        result = get_block(node->right, targetPtr, depthExp - 1, rightMemPtr);
    }
    return result;
}

/**
 * \brief Allocate a specific block in the buddy tree.
 * This function allocates a block of memory at a specific location in the buddy tree.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetExp The exponent of the block size to allocate (2^targetExp).
 * \param targetPtr Pointer to the target memory location.
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memPtr Pointer to the memory location that the current node manages.
 * \return Pointer to the allocated memory, or nullptr if allocation fails.
 */
unsigned int *Buddy::allocateSpecific(Node *node, unsigned int targetExp, unsigned int *targetPtr, unsigned int depthExp, unsigned int *memPtr) {
    if (node->unusable) return nullptr;

    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int memPtrValue = reinterpret_cast<unsigned int>(memPtr);
    unsigned int* leftMemPtr = memPtr;
    unsigned int* rightMemPtr = reinterpret_cast<unsigned int*>(memPtrValue + local_offset);

    if (depthExp == targetExp+1){
        if (!node->left){
            node->left = new Node();
            node->left->parent = node;
            return leftMemPtr; // Allocate in the left child
        }else if(!node->right){
            node->right = new Node();
            node->right->parent = node;
            return rightMemPtr; // Allocate in the right child
        }else{
            return nullptr; // If both children exist, allocation fails
        }
    }

    
    
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

/**
 * Print the buddy tree in a human-readable format.
 * This function prints the structure of the buddy tree, showing whether each block is free, occupied, or unusable.
 * \param prefix The prefix string for formatting the output.
 * \param node Pointer to the current node in the buddy tree.
 * \param isLeft Indicates whether the current node is a left child.
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memLocation Pointer to the memory location of the current node.
 */
void Buddy::printBT(const std::string& prefix, const Node* node, bool isLeft, unsigned int depthExp, unsigned int* memLocation)
{
    if( node != nullptr )
    {
        std::cout << prefix;

        std::cout << (isLeft ? "├──" : "└──" );

        // print the value of the node
        if(node == root){
            if(isRootOccupied) {
                std::cout << depthExp << " OCCUPIED " << memLocation << std::endl;
            }else{
                std::cout << depthExp << " FREE " << memLocation << std::endl;
            } 
        }else{
            if (node->unusable) {
                std::cout << depthExp << " UNUSABLE " << memLocation << std::endl;
            } else if (!node->left && !node->right) {
                std::cout << depthExp << " OCCUPIED " << memLocation << std::endl;
            } else {
                std::cout << depthExp << " FREE " << memLocation << std::endl;
            }
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

/**
 * Print the buddy allocator structure.
 * This function prints the entire buddy tree starting from the root node.
 */
void Buddy::printBuddy()
{
    printBT("", root, false, maxBlockExp, alignedBase);  
}

