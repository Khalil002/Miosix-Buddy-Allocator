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
     * \return Pointer to the reallocated memory block, or nullptr if reallocation fails.
     */
    unsigned int *reallocate(unsigned int *ptr, unsigned int new_size);

    /**
     * \brief Print the buddy allocator (for debugging).
     */
    void printBuddy();
private:

    class Node 
    {
    public:
        Node* left;
        Node* right;
        Node* parent;
        bool unusable;
        bool occupied;
        Node() : parent(nullptr), left(nullptr), right(nullptr), unusable(false), occupied(false) {}
    };

    unsigned int ceiling_log2(unsigned int x);
    void destroyTree(Node* node);
    void allocateVirtualBlocks(Node *node, unsigned int blockExp, unsigned int depthExp);
    unsigned int *allocateRecursive(Node *node, unsigned int blockExp, unsigned int depth, unsigned int *memLocation);
    void deallocateRecursive(Node *node, unsigned int depthExp, unsigned int* memLocation, unsigned int *ptr);
    void backPropagateDeallocate(Node *node);
    unsigned int get_exp_of_block(Node *node, unsigned int depthExp, unsigned int* memLocation, unsigned int *ptr);
    unsigned int *allocateSpecific(Node *node, unsigned int blockExp, unsigned int depthExp, unsigned int* memLocation, unsigned int *ptr);
    void printBT(const std::string& prefix, const Node* node, bool isLeft, unsigned int depthExp, unsigned int* memLocation);

    Node* root;
    unsigned int *memBase;
    unsigned int memSize;
    unsigned int minBlockExp;
    unsigned int maxBlockExp;
    unsigned int minBlockSize;
    unsigned int maxBlockSize;
    unsigned int offset;
    unsigned int *alignedBase;
    unsigned int alignedSize;
};