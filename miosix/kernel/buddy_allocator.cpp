#include "buddy_allocator.h"
#include <stdexcept>
#ifdef TEST_ALLOC
#include <iostream>
#endif
#include <stack>

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
            node = node->right; // Move to the right child
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
    //DestroyTree(root);
    destroyTreeIterative(root);
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

    if(isRootOccupied) return make_pair(nullptr, blockSize);

    // Root case
    if(blockExp == maxBlockExp) {
        if(isRootUnusable || root->left || root->right) return make_pair(nullptr, blockSize);
        isRootOccupied = true;
        return make_pair(alignedBase, blockSize);
    }

    //unsigned int *ptr = allocate(root, blockExp, maxBlockExp, reinterpret_cast<unsigned int>(alignedBase));
    unsigned int *ptr = allocateIterative(blockExp);
    return make_pair(ptr, blockSize);
}

/**
 * \brief Recursively allocate a memory block in the buddy tree.
 * This function traverses the buddy tree to find a suitable block for allocation.
 * \param node Pointer to the current node in the buddy tree.
 * \param targetExp The exponent of the block size to allocate (2^targetExp).
 * \param depthExp The current depth exponent in the buddy tree.
 * \param memPtrValue Value of the pointer to the memory location that the current node manages.
 * \param newNode Indicates whether the current node is a new node.
 * \return Pointer to the allocated memory, or nullptr if allocation fails.
 */
unsigned int *Buddy::allocate(Node *node, unsigned int targetExp, unsigned int depthExp, unsigned int memPtrValue, bool newNode){
    if (node->unusable) return nullptr;
    if(!newNode && !node->left && !node->right && targetExp+1<maxBlockExp) return nullptr; // If the parent of the target block has no children but is not a new node, this means the parent node is allocated

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
    
    bool newNode2 = newNode;
    if (!node->left){
        node->left = new Node();
        newNode2 = true;
    }
    unsigned int* leftResult = allocate(node->left, targetExp, depthExp - 1, leftMemPtrValue, newNode2);
    if (leftResult) return leftResult;

    newNode2 = newNode; // Reset newNode2 for the right child
    if (!node->right){
        node->right = new Node();
        newNode2 = true;
    }
    return allocate(node->right, targetExp, depthExp - 1, rightMemPtrValue, newNode2);
}

unsigned int *Buddy::allocateIterative(unsigned int targetExp) {
    
    stack<Frame> s;
    s.push({root, maxBlockExp, reinterpret_cast<unsigned int>(alignedBase), false});
    unsigned int *result = nullptr;

    while (!s.empty()) {
        Frame& f = s.top();
        s.pop();
        printf("Processing node at depth %u with pointer %x\n", f.depth, f.ptr);
        if (f.node->unusable 
            || (!f.newNode && !f.node->left && !f.node->right && f.depth > targetExp + 1)) {
            continue;
        }


        unsigned int local_offset = 1 << (f.depth - 1);
        unsigned int leftPtr = f.ptr;
        unsigned int rightPtr = f.ptr + local_offset;

        // Base case: we're one level above the target
        if (f.depth == targetExp + 1) {
            if (!f.node->left) {
                f.node->left = new Node();
                result = reinterpret_cast<unsigned int*>(leftPtr);
                break;
            } else if (!f.node->right) {
                f.node->right = new Node();
                result = reinterpret_cast<unsigned int*>(rightPtr);
                break;
            } else {
                continue;
            }
        }

        unsigned int depth = f.depth - 1;
        if (!f.node->left) {
            f.node->left = new Node();
            s.push({f.node->left, depth, leftPtr, true});
        } else {
            s.push({f.node->left, depth, leftPtr, false});
        }

        if (!f.node->right) {
            f.node->right = new Node();
            s.push({f.node->right, depth, rightPtr, true});
        } else {
            s.push({f.node->right, depth, rightPtr, false});
        }

        

        
    }
    
    return result; // If no suitable block was found, return nullptr
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

    if(!found) return 0; // If the target node was not found, do nothing

    // If the target node is the root, mark it as not occupied 
    if(depthExp == maxBlockExp) {
        isRootOccupied = false;
        return depthExp;
    }

    if(isLeftChild){
        //destroyTree(parent->left);
        destroyTreeIterative(parent->left);
        parent->left = nullptr;
    } else {
        //destroyTree(parent->right);
        destroyTreeIterative(parent->right);
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
    if(oldBlockExp==0) {
        throw invalid_argument("Pointer does not point to a valid block.");
    }
    
    // Allocate a new block with the requested size
    pair<unsigned int*, unsigned int> newBlock = allocate(newSize);
    unsigned int *newBlockPtr = newBlock.first;

    // If allocation failed, allocate the deallocated block
    if (!newBlockPtr) {
        //newBlockPtr = allocateSpecific(root, oldBlockExp, ptrValue, maxBlockExp, alignedBaseValue);
        newBlockPtr = allocateSpecificIterative(oldBlockExp, ptrValue);
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
 * @param newNode Indicates whether the current node is a new node.
 * \return Pointer to the allocated memory, or nullptr if allocation fails.
 */
unsigned int *Buddy::allocateSpecific(Node *node, unsigned int targetExp, unsigned int targetPtrValue, unsigned int depthExp, unsigned int memPtrValue, bool newNode) {
    if (node->unusable) return nullptr;
    if(!newNode && !node->left && !node->right && targetExp+1<maxBlockExp) return nullptr;

    unsigned int leftMemPtrValue = memPtrValue;
    unsigned int local_offset = 1 << (depthExp - 1); // size of half the block
    unsigned int rightMemPtrValue  = memPtrValue + local_offset;

    if (depthExp == targetExp+1){
        if (!node->left && leftMemPtrValue == targetPtrValue) {
            node->left = new Node();
            return reinterpret_cast<unsigned int*>(leftMemPtrValue);
        }else if(!node->right && rightMemPtrValue == targetPtrValue) {
            node->right = new Node();
            return reinterpret_cast<unsigned int*>(rightMemPtrValue);
        }else{
            return nullptr; // If both children exist, allocation fails
        }
    }

    bool newNode2 = newNode;
    if(leftMemPtrValue <= targetPtrValue && rightMemPtrValue  > targetPtrValue) {
        if (!node->left){
            node->left = new Node();
            newNode2 = true;
        }
        return allocateSpecific(node->left, targetExp, targetPtrValue, depthExp - 1, leftMemPtrValue, newNode2);
    }else{
        if (!node->right){
            node->right = new Node();
            newNode2 = true;
        }
        return allocateSpecific(node->right, targetExp, targetPtrValue, depthExp - 1, rightMemPtrValue, newNode2);
    }
}

unsigned int *Buddy::allocateSpecificIterative(unsigned int targetExp, unsigned int targetPtr) {
    
    stack<Frame> s;
    s.push({root, maxBlockExp, reinterpret_cast<unsigned int>(alignedBase), false});
    unsigned int *result = nullptr;

    while (!s.empty()) {
        Frame& f = s.top();
        s.pop();

        if (f.node->unusable 
            || (!f.newNode && !f.node->left && !f.node->right && f.depth > targetExp + 1)) {
            continue;
        }

        unsigned int local_offset = 1 << (f.depth - 1);
        unsigned int leftPtr = f.ptr;
        unsigned int rightPtr = f.ptr + local_offset;

        // Base case: we're one level above the target
        if (f.depth == targetExp + 1) {
            if (!f.node->left && leftPtr == targetPtr) {
                f.node->left = new Node();
                result = reinterpret_cast<unsigned int*>(leftPtr);
                break;
            }else if(!f.node->right && rightPtr == targetPtr) {
                f.node->right = new Node();
                result = reinterpret_cast<unsigned int*>(rightPtr);
                break;
            } else {
                continue;
            }
        }

        if(leftPtr <= targetPtr && rightPtr  > targetPtr) {
            if (!f.node->left) {
                f.node->left = new Node();
                s.push({f.node->left, f.depth - 1, leftPtr, true});
            } else {
                s.push({f.node->left, f.depth - 1, leftPtr, false});
            }
        }else{
            if (!f.node->right) {
                f.node->right = new Node();
                s.push({f.node->right, f.depth - 1, rightPtr, true});
            } else {
                s.push({f.node->right, f.depth - 1, rightPtr, false});
            }
        }
    }
    
    return result; // If no suitable block was found, return nullptr
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

void Buddy::destroyTreeIterative(Node* node)
{
    stack<Node*> s;
    s.push(node);
    while (!s.empty()) {
        Node* current = s.top();
        s.pop();
        if (current->left) s.push(current->left);
        if (current->right) s.push(current->right);
        delete current;
    }
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
                cout << depthExp << " " << memLocation << endl;
            } 
        }else{
            if (node->unusable) {
                cout << depthExp << " UNUSABLE " << memLocation << endl;
            } else if (!node->left && !node->right) {
                cout << depthExp << " OCCUPIED " << memLocation << endl;
            } else {
                cout << depthExp << " " << memLocation << endl;
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