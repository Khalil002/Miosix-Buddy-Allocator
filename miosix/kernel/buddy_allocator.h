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

    struct Frame {
        Node* node;
        unsigned int depth;
        unsigned int ptr;
        bool newNode;
    };

    unsigned int ceiling_log2(unsigned int x);
    unsigned int *allocate(Node *node, unsigned int targetExp, unsigned int depthExp, unsigned int memPtrValue, bool newNode=false);
    unsigned int *allocateIterative(unsigned int targetExp);
    unsigned int *allocateSpecific(Node *node, unsigned int targetExp, unsigned int targetPtrValue, unsigned int depthExp, unsigned int memPtrValue, bool newNode=false);
    unsigned int *allocateSpecificIterative(unsigned int targetExp, unsigned int targetPtrValue);
    unsigned int deallocate(unsigned int ptr);
    void destroyTree(Node* node);
    void destroyTreeIterative(Node* node);
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