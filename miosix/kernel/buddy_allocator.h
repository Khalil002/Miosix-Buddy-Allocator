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
     */
    Buddy(unsigned int *memBase, unsigned int memSize, unsigned int minBlockExp=10);
    
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

    #ifdef TEST_ALLOC
    /**
     * \brief Print the metadata of the buddy allocator.
     * This function prints the memory pool base address, size, aligned base address, aligned size,
     * minimum and maximum block sizes.
     */
    void printMetadata() const;

    /**
     * \brief Print the buddy allocator tree.
     */
    void printBuddy();
    #endif //TEST_ALLOC

private:

    class Node 
    {
    public:
        Node* left;
        Node* right;
        bool unusable;
        Node() : left(nullptr), right(nullptr), unusable(false) {}
    };

    unsigned int ceiling_log2(unsigned int x);
    unsigned int *allocate(Node *node, unsigned int targetExp, unsigned int depthExp, unsigned int memPtrValue);
    unsigned int *allocateSpecific(Node *node, unsigned int targetExp, unsigned int targetPtrValue, unsigned int depthExp, unsigned int memPtrValue);
    unsigned int deallocate(unsigned int ptr);
    void destroyTree(Node* node);
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
    bool isRootOccupied; // Indicates if the root (the block that covers the whole memory pool) is occupied
    bool isRootUnusable; // Indicates if the root (the block that covers the whole memory pool) is unusable
    Node* root;
};