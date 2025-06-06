#pragma once
#include <string>

class Buddy 
{
public:
    /**
     * Constructor.
     * \param memBase address of the start of the memory pool.
     * \param memSize size of the memory pool.
     */
    Buddy(unsigned int *memBase, unsigned int memSize);
    
    /**
     * Destructor
     */
    ~Buddy();

    /**
     * \brief Allocate memory of the given size using the buddy allocator.
     * \param size The size of memory to allocate in bytes.
     * \return Pointer to the allocated memory, or nullptr if allocation fails.
     */
    std::pair<unsigned int *, unsigned int>allocate(unsigned int size);

    /**
     * \brief Free the allocated memory block.
     * \param ptr Pointer to the memory to free.
     */
    void deallocate(unsigned int *ptr);

    /**
     * \brief Reallocate a memory block into a block of the given size (data is not copied).
     * \param ptr Pointer to the memory block.
     * \param new_size The new size of the memory block in bytes.
     * \return Pointer to the reallocated memory block, in case of failure it returns the original pointer.
     */
    unsigned int *reallocate(unsigned int *ptr, unsigned int new_size);

    /**
     * \brief Print the buddy allocator (for debugging).
     */
    void printBuddy();

    unsigned int *memBase;
    unsigned int memSize;
    unsigned int minBlockExp;
    unsigned int maxBlockExp;
    unsigned int minBlockSize;
    unsigned int maxBlockSize;
    unsigned int offset;
    unsigned int *alignedBase;
    unsigned int alignedSize;
private:

    class Node 
    {
    public:
        Node* left;
        Node* right;
        Node* parent;
        bool unusable;
        Node() : parent(nullptr), left(nullptr), right(nullptr), unusable(false) {}
    };

    unsigned int ceiling_log2(unsigned int x);
    void destroyTree(Node* node);
    void allocateUnusableBlock(Node *node, unsigned int blockExp, unsigned int depthExp);
    unsigned int *allocateRecursive(Node *node, unsigned int blockExp, unsigned int depth, unsigned int *memPtr);
    void deallocateRecursive(Node *node, unsigned int *targetPtr, unsigned int depthExp, unsigned int* memPtr);
    void backPropagateDeallocate(Node *node);
    std::pair<Node *, unsigned int> get_block(Node *node, unsigned int *targetPtr, unsigned int depthExp, unsigned int* memPtr);
    unsigned int *allocateSpecific(Node *node, unsigned int targetExp, unsigned int *targetPtr, unsigned int depthExp, unsigned int *memPtr);
    void printBT(const std::string& prefix, const Node* node, bool isLeft, unsigned int depthExp, unsigned int* memLocation);

    
    bool isRootOccupied; // Indicates if the root (the block that covers the whole memory pool) is occupied
    bool isRootUnusable; // Indicates if the root (the block that covers the whole memory pool) is unusable
    Node* root;
};