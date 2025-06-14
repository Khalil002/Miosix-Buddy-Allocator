#include "buddy_allocator.h"
#include <stdexcept>
#ifdef TEST_ALLOC
#include <iostream>
#endif

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
  * \param minBlockExp Exponent for the minimum block size (default is 10, which means 1024 bytes).
  * \throws invalid_argument if the memory size is smaller than the minimum block size.
  */
Buddy::Buddy(unsigned int *memBase, unsigned int memSize, unsigned int minBlockExp)
{
    this->memBase = memBase; // Base address of the memory pool
    this->memSize = memSize; // Size of the memory pool in bytes
    this->minBlockExp = minBlockExp; // Minimum block size exponent
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
        this->isRootUnusable = true; //The entire memory pool is not allowable for allocation
        unsigned int unusableSize = maxBlockSize-alignedSize;
        unsigned int unusableExp = ceiling_log2(unusableSize);
        unsigned int n = maxBlockExp - unusableExp;
        Node *node = root;
        for(unsigned int i = 0; i < n; i++){
            node->right = new Node();
        }
        node->unusable = true;
    }
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
        if(isRootOccupied || isRootUnusable || root->left || root->right) return make_pair(nullptr, blockSize);
        isRootOccupied = true; // Mark the root as occupied
        return make_pair(alignedBase, blockSize); // Return the aligned base address
    }

    unsigned int *ptr = allocate(root, blockExp, maxBlockExp, reinterpret_cast<unsigned int>(alignedBase));
    return make_pair(ptr, blockSize);
}

/**
 * \brief Recursively allocate a memory block in the buddy tree.
 * This function traverses the buddy tree to find a suitable block for allocation.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetExp The exponent of the block size to allocate (2^targetExp).
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memPtrValue Value of the pointer to the memory location that the current node manages.
 * \return Pointer to the allocated memory, or nullptr if allocation fails.
 */
unsigned int *Buddy::allocate(Node *node, unsigned int targetExp, unsigned int depthExp, unsigned int memPtrValue){
    if (node->unusable) return nullptr;

    unsigned int leftMemPtrValue = memPtrValue;
    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int rightMemPtrValue = memPtrValue + local_offset;

    if (depthExp == targetExp+1){
        if (!node->left){
            node->left = new Node();
            return reinterpret_cast<unsigned int*>(leftMemPtrValue);
        }else if(!node->right){
            node->right = new Node();
            return reinterpret_cast<unsigned int*>(rightMemPtrValue);
        }else{
            return nullptr; // If both children exist, allocation fails
        }
    }
    
    if (!node->left) node->left = new Node();
    unsigned int* leftResult = allocate(node->left, targetExp, depthExp - 1, leftMemPtrValue);
    if (leftResult) return leftResult;

    if (!node->right) node->right = new Node();
    return allocate(node->right, targetExp, depthExp - 1, rightMemPtrValue);
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

    // Check if the pointer is aligned to the minimum block size
    unsigned int ptrValue = reinterpret_cast<unsigned int>(ptr);
    if (ptrValue % minBlockSize != 0) {
        throw invalid_argument("Pointer is not aligned to the minimum block size.");
    }

    // Check if the pointer is within the bounds of the memory pool
    unsigned int alignedBaseValue = reinterpret_cast<unsigned int>(alignedBase);
    if(ptrValue < alignedBaseValue || ptrValue >= (alignedBaseValue + alignedSize)) {
        throw invalid_argument("Pointer is out of bounds of the memory pool.");
    }

    //deallocateRecursive(root, ptr, maxBlockExp, alignedBase);
    deallocate(ptrValue); // Use iterative deallocation (no parent pointer needed)
}

/**
 * \brief Deallocate a memory block pointed to by the given pointer.
 * \param ptrValue The value of the pointer to the memory block to deallocate.
 * \return The depth exponent of the block after deallocation.
 */
unsigned int Buddy::deallocate(unsigned int ptrValue) {
    Node* node = root;
    Node* parent = root;
    bool isLeftChild = true;
    bool found = false;
    unsigned int depthExp = maxBlockExp;
    unsigned int targetMem = ptrValue;
    unsigned int currentMem = reinterpret_cast<unsigned int>(alignedBase);
    
    while(true){
        if(!node || node->unusable || depthExp<minBlockExp) break;

        if(!node->left && !node->right && currentMem == targetMem) {
            found = true; // Found the target node
            break;
        }

        unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
        unsigned int leftMem = currentMem;
        unsigned int rightMem = currentMem + local_offset;

        if(leftMem <= targetMem && rightMem > targetMem) {
            if(node->right){
                parent = node;
                isLeftChild = true;
            }
            currentMem = leftMem; // Update current memory location
            node = node->left; // Move to the left child
        } else {
            if(node->left){
                parent = node;
                isLeftChild = false;
            }
            currentMem = rightMem; // Update current memory location
            node = node->right; // Move to the right child
        }
        depthExp--; // Decrease the depth exponent
    }

    if(!found) return depthExp; // If the target node was not found, do nothing

    // If the target node is the root, mark it as not occupied 
    if(depthExp == maxBlockExp) {
        isRootOccupied = false;
        return depthExp;
    }

    if(isLeftChild){
        destroyTree(parent->left);
        parent->left = nullptr;
    } else {
        destroyTree(parent->right);
        parent->right = nullptr; 
    }
    return depthExp;
}

/**
 * \brief Reallocate a memory block into a block of the given size (data is not copied).
 * \param ptr Pointer to the memory block.
 * \param newSize The new size of the memory block in bytes.
 * \return Pointer to the reallocated memory block, in case of failure it returns the original pointer.
 * \throws invalid_argument if the pointer is null or out of bounds or not aligned.
 */
unsigned int *Buddy::reallocate(unsigned int *ptr, unsigned int newSize){
    if (!ptr) throw invalid_argument("Pointer is null.");

    if (newSize < minBlockSize || newSize > maxBlockSize) {
        throw invalid_argument("Invalid allocation size.");
    }

    // Calculate the offset from the aligned base address
    unsigned int ptrValue = reinterpret_cast<unsigned int>(ptr);
    if (ptrValue % minBlockSize != 0) {
        throw invalid_argument("Pointer is not aligned to the minimum block size.");
    }

    // Check if the pointer is within the bounds of the memory pool
    unsigned int alignedBaseValue = reinterpret_cast<unsigned int>(alignedBase);
    if(ptrValue < alignedBaseValue || ptrValue >= (alignedBaseValue + alignedSize)) {
        throw invalid_argument("Pointer is out of bounds of the memory pool.");
    }
    
    // Deallocate the current block
    unsigned int oldBlockExp = deallocate(ptrValue);

    // Allocate a new block with the requested size
    pair<unsigned int*, unsigned int> newBlock = allocate(newSize);
    unsigned int *newBlockPtr = newBlock.first;

    // If allocation failed, allocate the deallocated block
    if (!newBlockPtr) {
        newBlockPtr = allocateSpecific(root, oldBlockExp, ptrValue, maxBlockExp, alignedBaseValue);
    }

    return newBlockPtr;
}

/**
 * \brief Allocate a specific block in the buddy tree.
 * This function allocates a block of memory at a specific location in the buddy tree.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetExp The exponent of the block size to allocate (2^targetExp).
 * \param targetPtrValue Value of the Pointer to the target memory location.
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memPtrValue Value of the Pointer to the memory location that the current node manages.
 * \return Pointer to the allocated memory, or nullptr if allocation fails.
 */
unsigned int *Buddy::allocateSpecific(Node *node, unsigned int targetExp, unsigned int targetPtrValue, unsigned int depthExp, unsigned int memPtrValue) {
    if (node->unusable) return nullptr;

    unsigned int leftMemPtrValue = memPtrValue;
    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int rightMemPtrValue  = memPtrValue + local_offset;

    if (depthExp == targetExp+1){
        if (!node->left && memPtrValue == leftMemPtrValue) {
            node->left = new Node();
            return reinterpret_cast<unsigned int*>(leftMemPtrValue);
        }else if(!node->right && memPtrValue == rightMemPtrValue) {
            node->right = new Node();
            return reinterpret_cast<unsigned int*>(rightMemPtrValue);
        }else{
            return nullptr; // If both children exist, allocation fails
        }
    }

    if(leftMemPtrValue <= targetPtrValue && rightMemPtrValue  > targetPtrValue) {
        if (!node->left) node->left = new Node();
        return allocateSpecific(node->left, targetExp, targetPtrValue, depthExp - 1, leftMemPtrValue);
    }else{
        if (!node->right) node->right = new Node();
        return allocateSpecific(node->right, targetExp, targetPtrValue, depthExp - 1, rightMemPtrValue);
    }
}

/*
 * Calculate the ceiling of log base 2 of a number.
 * This function returns the smallest uint e such that 2^e >= x.
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
 * Destroy the buddy tree/subtree recursively.
 * This function deletes all nodes in the buddy tree/subtree to free memory.
 * \param node Pointer to the current node in the buddy tree.
 */
void Buddy::destroyTree(Node* node)
{
    if (node == nullptr) return;
    destroyTree(node->left);
    destroyTree(node->right);
    delete node;
}

#ifdef TEST_ALLOC
/**
 * Print the buddy tree in a human-readable format.
 * This function prints the structure of the buddy tree, showing whether each block is free, occupied, or unusable.
 * \param prefix The prefix string for formatting the output.
 * \param node Pointer to the current node in the buddy tree.
 * \param isLeft Indicates whether the current node is a left child.
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memLocation Pointer to the memory location of the current node.
 */
void Buddy::printBT(const string& prefix, const Node* node, bool isLeft, unsigned int depthExp, unsigned int* memLocation)
{
    if( node != nullptr )
    {
        cout << prefix;

        cout << (isLeft ? "├──" : "└──" );

        // print the value of the node
        if(node == root){
            if(isRootOccupied) {
                cout << depthExp << " OCCUPIED " << memLocation << endl;
            }else{
                cout << depthExp << " FREE " << memLocation << endl;
            } 
        }else{
            if (node->unusable) {
                cout << depthExp << " UNUSABLE " << memLocation << endl;
            } else if (!node->left && !node->right) {
                cout << depthExp << " OCCUPIED " << memLocation << endl;
            } else {
                cout << depthExp << " FREE " << memLocation << endl;
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

void Buddy::printMetadata() const {
    cout 
        << "memory pool initialized with base address: " << static_cast<void*>(memBase)
        << ", size: "      << memSize << " bytes" << endl

        << "offset: "      << offset  << " bytes" << endl

        << "Aligned base address: " << static_cast<void*>(alignedBase)
        << ", Aligned size: "       << alignedSize << " bytes" << endl

        << "Minimum block size: 2^" << minBlockExp 
        << " = "                   << minBlockSize  << " bytes" << endl

        << "Maximum block size: 2^" << maxBlockExp 
        << " = "                   << maxBlockSize  << " bytes" << endl;
}
#endif //TEST_ALLOC