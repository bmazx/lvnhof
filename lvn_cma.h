#ifndef LVN_CUSTOM_MEMORY_ALLOCATOR_H
#define LVN_CUSTOM_MEMORY_ALLOCATOR_H

// debug
#ifdef LVN_CONFIG_DEBUG
    #ifndef LVN_DEBUG_ALLOC_VALUE
        #define LVN_DEBUG_ALLOC_VALUE (0xDD)
    #endif
    #ifndef LVN_DEBUG_FREE_VALUE
        #define LVN_DEBUG_FREE_VALUE (0xEE)
    #endif
#endif

#ifndef LVN_CMA_ASSERT
    #define LVN_CMA_ASSERT(x, str) assert((x) && str)
#endif

// memory
#if defined(LVN_CMA_MALLOC) && defined(LVN_CMA_FREE)
// ok
#elif !defined(LVN_CMA_MALLOC) && !defined(LVN_CMA_FREE)
    #define LVN_CMA_MALLOC(sz)    malloc(sz)
    #define LVN_CMA_FREE(ptr)     free(ptr)
#else
    #error "must define both LVN_CMA_MALLOC and LVN_CMA_FREE or neither"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// alignment
#if defined(__cplusplus) || (__STDC_VERSION__ >= 202000)
    #define LVN_ALIGNOF(T) alignof(T)
    typedef max_align_t lvn_max_align_t;
#elif defined(__STDC__) && (__STDC_VERSION__ >= 201112L)
    #define LVN_ALIGNOF(T) _Alignof(T)
    typedef max_align_t lvn_max_align_t;
#else
    #define LVN_ALIGNOF(T) ((size_t)offsetof(struct { char c; T x; }, x))
    typedef union {
        long long ll;
        long double ld;
        void* p;
    } lvn_max_align_t;
#endif

#define LVN_DEFAULT_ALIGN (LVN_ALIGNOF(lvn_max_align_t))
#define LVN_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define LVN_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define LVN_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)


typedef enum LvnResult
{
    Lvn_Result_Success =  0,
    Lvn_Result_Failure = -1,
    Lvn_Result_OutOfMemory = -2,
} LvnResult;

typedef struct LvnMemoryBlock
{
    struct LvnMemoryBlock* next;    /* ptr to the next memory block */
    uint8_t* allocation;            /* ptr to the start of the allocated block (return address of allocation from allocator) */
    uint8_t* allocAligned;          /* ptr to the start of the allocated block after aligned to a specific alignment (this address will used for custom allocs) */
    uint8_t* currIndex;             /* the current index/address to be used for allocating the next allocation from the block (will be set to allocAligned on init) */
    size_t size;                    /* the size of the block (note the actual allocation may be larger due to alignment requirements, size + align) */
} LvnMemoryBlock;

typedef struct LvnFreeNode
{
    struct LvnFreeNode* next;        /* ptr to the next ptr node address in the list */
} LvnFreeNode;

typedef struct LvnArenaMark
{
    LvnMemoryBlock* block;
    struct LvnArenaMark* next;
    size_t offset;
} LvnArenaMark;

typedef struct LvnMemoryPool
{
    LvnMemoryBlock* blocks;         /* list of blocks containing the allocated memory */
    LvnFreeNode* freeList;          /* node list of free memory addresses in the pool */
    size_t stride;                  /* the stride of the element in bytes in the pool (requested size) */
    size_t strideAligned;           /* the stride aligned to a multiple of align (actual size alloced by pool) */
    size_t align;                   /* the alignment multiple of the elements in bytes */

    size_t allocCount;              /* track allocations allocced from pool for debugging */
} LvnMemoryPool;

typedef struct LvnMemoryArena
{
    LvnMemoryBlock* blocks;         /* list of blocks containing the allocated memory */
    LvnArenaMark* marks;            /* list of arena marks in stack order for pushing/popping marks */
    size_t align;                   /* the alignment multiple of the allocation in bytes */

    size_t allocCount;              /* track allocations allocced from arena for debugging */
} LvnMemoryArena;

typedef struct LvnMemoryBlockCreateInfo
{
    size_t size;
    size_t align;
    LvnMemoryBlock* next;
} LvnMemoryBlockCreateInfo;

typedef struct LvnMemoryPoolCreateInfo
{
    size_t count;
    size_t stride;
    size_t align;
} LvnMemoryPoolCreateInfo;

typedef struct LvnMemoryArenaCreateInfo
{
    size_t size;
    size_t align;
} LvnMemoryArenaCreateInfo;


bool         lvn_ptrInBlock(uint8_t* block, size_t size, void* ptr);
LvnResult    lvn_memBlockCreate(LvnMemoryBlock** memBlock, const LvnMemoryBlockCreateInfo* createInfo);
void         lvn_memBlockDestroy(LvnMemoryBlock* memBlock);
void         lvn_memBlockDestroyChain(LvnMemoryBlock* memBlock);
size_t       lvn_memBlockGetSize(LvnMemoryBlock* memBlock);
size_t       lvn_memBlockGetOffset(LvnMemoryBlock* memBlock);
LvnResult    lvn_memPoolCreate(LvnMemoryPool* memPool, const LvnMemoryPoolCreateInfo* createInfo);
void         lvn_memPoolDestroy(LvnMemoryPool* memPool);
LvnResult    lvn_memPoolPushBlock(LvnMemoryPool* memPool, size_t count);
void*        lvn_memPoolAlloc(LvnMemoryPool* memPool);
void         lvn_memPoolFree(LvnMemoryPool* memPool, void* ptr);
void         lvn_memPoolReset(LvnMemoryPool* memPool);
LvnResult    lvn_memPoolResetMergeBlocks(LvnMemoryPool* memPool);
size_t       lvn_memPoolGetTotalCapacity(LvnMemoryPool* memPool);
size_t       lvn_memPoolGetAllocCount(LvnMemoryPool* memPool);
LvnResult    lvn_memArenaCreate(LvnMemoryArena* memArena, const LvnMemoryArenaCreateInfo* createInfo);
void         lvn_memArenaDestroy(LvnMemoryArena* memArena);
LvnResult    lvn_memArenaPushBlock(LvnMemoryArena* memArena, size_t size);
void*        lvn_memArenaAlloc(LvnMemoryArena* memArena, size_t size);
void*        lvn_memArenaAllocAligned(LvnMemoryArena* memArena, size_t size, size_t align);
LvnArenaMark lvn_memArenaMark(LvnMemoryArena* memArena);
void         lvn_memArenaMarkRevert(LvnMemoryArena* memArena, const LvnArenaMark* mark);
void         lvn_memArenaMarkPush(LvnMemoryArena* memArena);
void         lvn_memArenaMarkPop(LvnMemoryArena* memArena);
LvnResult    lvn_memArenaResetMergeBlocks(LvnMemoryArena* memArena);
size_t       lvn_memArenaGetTotalSize(LvnMemoryArena* memArena);
size_t       lvn_memArenaGetAllocCount(LvnMemoryArena* memArena);


#ifdef LVN_CMA_IMPL

#include <assert.h>
#include <stdlib.h>
#include <string.h>


bool lvn_ptrInBlock(uint8_t* block, size_t size, void* ptr)
{
    uint8_t* start = block;
    uint8_t* end = start + size;
    return (uint8_t*)ptr >= start && (uint8_t*)ptr < end;
}

LvnResult lvn_memBlockCreate(LvnMemoryBlock** memBlock, const LvnMemoryBlockCreateInfo* createInfo)
{
    LVN_CMA_ASSERT(memBlock && createInfo, "memBlock and createInfo cannot be null");
    LVN_CMA_ASSERT(createInfo->align != 0 && (createInfo->align & (createInfo->align - 1)) == 0, "align cannot be zero or a non power of two");
    LVN_CMA_ASSERT(createInfo->align >= LVN_ALIGNOF(lvn_max_align_t), "align must be >= default alignment");
    LVN_CMA_ASSERT(createInfo->size <= SIZE_MAX - createInfo->align, "size overflow, size + align must be <= SIZE_MAX");

    LvnResult errResult = Lvn_Result_Failure;

    *memBlock = (LvnMemoryBlock*) LVN_CMA_MALLOC(sizeof(LvnMemoryBlock));
    if (!*memBlock)
    {
        errResult = Lvn_Result_OutOfMemory;
        goto fail_cleanup;
    }

    memset(*memBlock, 0, sizeof(LvnMemoryBlock));

    LvnMemoryBlock* memBlockPtr = *memBlock;

    memBlockPtr->allocation = (uint8_t*) LVN_CMA_MALLOC(createInfo->size + createInfo->align);
    if (!memBlockPtr->allocation)
    {
        errResult = Lvn_Result_OutOfMemory;
        goto fail_cleanup;
    }

    memBlockPtr->allocAligned = (uint8_t*) LVN_ALIGN_UP((uintptr_t)memBlockPtr->allocation, createInfo->align);
    memBlockPtr->size = createInfo->size;
    memBlockPtr->currIndex = memBlockPtr->allocAligned;
    memBlockPtr->next = createInfo->next;

#ifdef LVN_CONFIG_DEBUG
    memset(memBlockPtr->allocation, LVN_DEBUG_ALLOC_VALUE, createInfo->size + createInfo->align);
#endif

    return Lvn_Result_Success;

fail_cleanup:
    if (*memBlock)
    {
        if ((*memBlock)->allocation) { LVN_CMA_FREE((*memBlock)->allocation); }

        LVN_CMA_FREE(*memBlock);
        *memBlock = NULL;
    }
    return errResult;
}

void lvn_memBlockDestroy(LvnMemoryBlock* memBlock)
{
    if (!memBlock) { return; }
    if (memBlock->allocation) { LVN_CMA_FREE(memBlock->allocation); }
    LVN_CMA_FREE(memBlock);
}

void lvn_memBlockDestroyChain(LvnMemoryBlock* memBlock)
{
    while (memBlock)
    {
        LvnMemoryBlock* temp = memBlock;
        memBlock = memBlock->next;
        LVN_CMA_FREE(temp->allocation);
        LVN_CMA_FREE(temp);
    }
}

size_t lvn_memBlockGetSize(LvnMemoryBlock* memBlock)
{
    LVN_CMA_ASSERT(memBlock, "memBlock cannot be null");
    return memBlock->size;
}

size_t lvn_memBlockGetOffset(LvnMemoryBlock* memBlock)
{
    LVN_CMA_ASSERT(memBlock, "memBlock cannot be null");
    return (uintptr_t)(memBlock->currIndex - memBlock->allocAligned);
}

LvnResult lvn_memPoolCreate(LvnMemoryPool* memPool, const LvnMemoryPoolCreateInfo* createInfo)
{
    LVN_CMA_ASSERT(memPool && createInfo, "memPool and createInfo cannot be null");
    LVN_CMA_ASSERT(createInfo->stride == 0 || createInfo->count <= SIZE_MAX / createInfo->stride, "overflow on creating memory size (count * stride)");
    LVN_CMA_ASSERT(createInfo->align != 0 && (createInfo->align & (createInfo->align - 1)) == 0, "align cannot be zero or a non power of two");
    LVN_CMA_ASSERT(createInfo->align >= LVN_ALIGNOF(lvn_max_align_t), "align must be >= pointer alignment");

    LvnResult errResult = Lvn_Result_Failure;
    LvnMemoryBlock* memBlock = NULL;

    // set min stride
    const size_t minStride = sizeof(void*);
    size_t stride = createInfo->stride < minStride ? minStride : createInfo->stride;

    // align stride
    size_t strideAligned = LVN_ALIGN_UP(stride, createInfo->align);

    LVN_CMA_ASSERT(strideAligned % createInfo->align == 0, "stride must be multiple of align");
    LVN_CMA_ASSERT(createInfo->count <= SIZE_MAX / strideAligned, "memory size overflow, count * strideAligned must be <= SIZE_MAX");

    // create memory block
    LvnMemoryBlockCreateInfo memBlockCreateInfo = {
        .size = createInfo->count * strideAligned,
        .align = createInfo->align,
        .next = NULL,
    };

    LvnResult result = lvn_memBlockCreate(&memBlock, &memBlockCreateInfo);
    if (result != Lvn_Result_Success)
    {
        errResult = result;
        goto fail_cleanup;
    }

    // fill memory pool info
    *memPool = (LvnMemoryPool){
        .blocks = memBlock,
        .freeList = NULL,
        .stride = createInfo->stride,
        .strideAligned = strideAligned,
        .align = createInfo->align,
        .allocCount = 0,
    };

    return Lvn_Result_Success;

fail_cleanup:
    if (memBlock) { lvn_memBlockDestroy(memBlock); }
    return errResult;
}

void lvn_memPoolDestroy(LvnMemoryPool* memPool)
{
    if (!memPool) { return; }

    LvnMemoryBlock* currBlock = memPool->blocks;
    while (currBlock)
    {
        LvnMemoryBlock* temp = currBlock;
        currBlock = currBlock->next;
        LVN_CMA_FREE(temp->allocation);
        LVN_CMA_FREE(temp);
    }

    memPool->blocks = NULL;
    memPool->freeList = NULL;
    memPool->stride = 0;
    memPool->strideAligned = 0;
    memPool->align = 0;
    memPool->allocCount = 0;
}

LvnResult lvn_memPoolPushBlock(LvnMemoryPool* memPool, size_t count)
{
    LVN_CMA_ASSERT(memPool, "memPool cannot be null");
    LVN_CMA_ASSERT(count <= SIZE_MAX / memPool->strideAligned, "memory size overflow, count * strideAligned must be <= SIZE_MAX");

    if (!count)
        return Lvn_Result_Success;

    LvnResult errResult = Lvn_Result_Failure;
    LvnMemoryBlock* memBlock = NULL;

    LvnMemoryBlockCreateInfo memBlockCreateInfo = {
        .size = count * memPool->strideAligned,
        .align = memPool->align,
        .next = memPool->blocks,
    };

    LvnResult result = lvn_memBlockCreate(&memBlock, &memBlockCreateInfo);
    if (result != Lvn_Result_Success)
    {
        errResult = result;
        goto fail_cleanup;
    }

    memPool->blocks = memBlock;

    return Lvn_Result_Success;

fail_cleanup:
    if (memBlock) { lvn_memBlockDestroy(memBlock); }
    return errResult;
}

void* lvn_memPoolAlloc(LvnMemoryPool* memPool)
{
    LVN_CMA_ASSERT(memPool, "memPool cannot be null");

    void* ptr = NULL;

    // check free list
    if (memPool->freeList)
    {
        ptr = memPool->freeList;
        memPool->freeList = memPool->freeList->next;
        goto alloc_success;
    }

    // get next memory block index in pool if available
    for (LvnMemoryBlock* currBlock = memPool->blocks; currBlock; currBlock = currBlock->next)
    {
        if ((currBlock->currIndex + memPool->strideAligned) <= (currBlock->allocAligned + currBlock->size))
        {
            ptr = currBlock->currIndex;
            currBlock->currIndex += memPool->strideAligned;
            goto alloc_success;
        }
    }

    // create new memory block if no space left
    if (lvn_memPoolPushBlock(memPool, lvn_memPoolGetTotalCapacity(memPool)) != Lvn_Result_Success)
        return NULL;

    if ((memPool->blocks->currIndex + memPool->strideAligned) <= (memPool->blocks->allocAligned + memPool->blocks->size))
    {
        ptr = memPool->blocks->currIndex;
        memPool->blocks->currIndex += memPool->strideAligned;
        goto alloc_success;
    }

    // unable to find next block index
    return NULL;

alloc_success:
    memPool->allocCount++;
    return ptr;
}

void lvn_memPoolFree(LvnMemoryPool* memPool, void* ptr)
{
    LVN_CMA_ASSERT(memPool, "memPool cannot be null");

    if (!ptr) { return; }

    // find block the ptr was allocated from
    LvnMemoryBlock* currBlock = NULL;
    for (currBlock = memPool->blocks; currBlock; currBlock = currBlock->next)
    {
        if (lvn_ptrInBlock(currBlock->allocAligned, currBlock->size, ptr))
            break;
    }
    LVN_CMA_ASSERT(currBlock, "ptr not found within memory pool blocks");

    // check alignment of pointer
    size_t offset = (uint8_t*)ptr - (uint8_t*)currBlock->allocAligned;
    LVN_CMA_ASSERT(offset % memPool->strideAligned == 0, "invalid pool pointer, pointer not aligned to pool stride align");

#ifdef LVN_CONFIG_DEBUG
    // checks if ptr was already freed (double free)
    for (LvnFreeNode* node = memPool->freeList; node; node = node->next)
    {
        LVN_CMA_ASSERT(node != ptr, "double free in memory pool");
    }
    memset(ptr, LVN_DEBUG_FREE_VALUE, memPool->strideAligned);
#endif

    // check if ptr is trying to be freed after pool reset/create
    LVN_CMA_ASSERT(memPool->allocCount > 0, "cannot free ptr to pool, pool was probably reset or just created");
    memPool->allocCount--;

    // free ptr, add to free list
    LvnFreeNode* node = (LvnFreeNode*) ptr;
    node->next = memPool->freeList;
    memPool->freeList = node;
}

void lvn_memPoolReset(LvnMemoryPool* memPool)
{
    LVN_CMA_ASSERT(memPool, "memPool cannot be null");

    for (LvnMemoryBlock* currBlock = memPool->blocks; currBlock; currBlock = currBlock->next)
    {
        currBlock->currIndex = currBlock->allocAligned;
#ifdef LVN_CONFIG_DEBUG
        memset(currBlock->allocAligned, LVN_DEBUG_FREE_VALUE, currBlock->size);
#endif
    }

    memPool->freeList = NULL;
    memPool->allocCount = 0;
}

LvnResult lvn_memPoolResetMergeBlocks(LvnMemoryPool* memPool)
{
    LVN_CMA_ASSERT(memPool, "memPool cannot be null");

    LvnResult errResult = Lvn_Result_Failure;
    LvnMemoryBlock* memBlock = NULL;

    size_t totalSize = 0;
    for (LvnMemoryBlock* currBlock = memPool->blocks; currBlock; currBlock = currBlock->next)
        totalSize += currBlock->size;

    LvnMemoryBlockCreateInfo memBlockCreateInfo = {
        .size = totalSize,
        .align = memPool->align,
        .next = NULL,
    };

    LvnResult result = lvn_memBlockCreate(&memBlock, &memBlockCreateInfo);
    if (result != Lvn_Result_Success)
    {
        errResult = result;
        goto fail_cleanup;
    }

    LvnMemoryBlock* currBlock = memPool->blocks;
    while (currBlock)
    {
        LvnMemoryBlock* temp = currBlock;
        currBlock = currBlock->next;
        LVN_CMA_FREE(temp->allocation);
        LVN_CMA_FREE(temp);
    }

    memPool->blocks = memBlock;

    return Lvn_Result_Success;

fail_cleanup:
    if (memBlock) { lvn_memBlockDestroy(memBlock); }
    return errResult;
}

size_t lvn_memPoolGetTotalCapacity(LvnMemoryPool* memPool)
{
    LVN_CMA_ASSERT(memPool, "memPool cannot be null");

    size_t count = 0;

    for (LvnMemoryBlock* currBlock = memPool->blocks; currBlock; currBlock = currBlock->next)
        count += currBlock->size / memPool->strideAligned;

    return count;
}

size_t lvn_memPoolGetAllocCount(LvnMemoryPool* memPool)
{
    LVN_CMA_ASSERT(memPool, "memPool cannot be null");
    return memPool->allocCount;
}

LvnResult lvn_memArenaCreate(LvnMemoryArena* memArena, const LvnMemoryArenaCreateInfo* createInfo)
{
    LVN_CMA_ASSERT(memArena && createInfo, "memArena and createInfo cannot be null");
    LVN_CMA_ASSERT(createInfo->size <= SIZE_MAX, "arena size overflow");
    LVN_CMA_ASSERT(createInfo->align != 0 && (createInfo->align & (createInfo->align - 1)) == 0, "align cannot be zero or a non power of two");
    LVN_CMA_ASSERT(createInfo->align >= LVN_ALIGNOF(lvn_max_align_t), "align must be >= pointer alignment");

    LvnResult errResult = Lvn_Result_Failure;
    LvnMemoryBlock* memBlock = NULL;

    // create memory block
    LvnMemoryBlockCreateInfo memBlockCreateInfo = {
        .size = createInfo->size,
        .align = createInfo->align,
        .next = NULL,
    };

    LvnResult result = lvn_memBlockCreate(&memBlock, &memBlockCreateInfo);
    if (result != Lvn_Result_Success)
    {
        errResult = result;
        goto fail_cleanup;
    }

    // fill memory arena info
    *memArena = (LvnMemoryArena){
        .blocks = memBlock,
        .marks = NULL,
        .align = createInfo->align,
        .allocCount = 0,
    };

    return Lvn_Result_Success;

fail_cleanup:
    if (memBlock) { lvn_memBlockDestroy(memBlock); }
    return errResult;
}

void lvn_memArenaDestroy(LvnMemoryArena* memArena)
{
    if (!memArena) { return; }

    LvnMemoryBlock* currBlock = memArena->blocks;
    while (currBlock)
    {
        LvnMemoryBlock* temp = currBlock;
        currBlock = currBlock->next;
        LVN_CMA_FREE(temp->allocation);
        LVN_CMA_FREE(temp);
    }

    LvnArenaMark* currMark = memArena->marks;
    while (currMark)
    {
        LvnArenaMark* temp = currMark;
        currMark = currMark->next;
        LVN_CMA_FREE(temp);
    }

    memArena->blocks = NULL;
    memArena->marks = NULL;
    memArena->align = 0;
    memArena->allocCount = 0;
}

LvnResult lvn_memArenaPushBlock(LvnMemoryArena* memArena, size_t size)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");

    if (!size)
        return Lvn_Result_Success;

    LvnResult errResult = Lvn_Result_Failure;
    LvnMemoryBlock* memBlock = NULL;

    LvnMemoryBlockCreateInfo memBlockCreateInfo = {
        .size = size,
        .align = memArena->align,
        .next = memArena->blocks,
    };

    LvnResult result = lvn_memBlockCreate(&memBlock, &memBlockCreateInfo);
    if (result != Lvn_Result_Success)
    {
        errResult = result;
        goto fail_cleanup;
    }

    memArena->blocks = memBlock;

    return Lvn_Result_Success;

fail_cleanup:
    if (memBlock) { lvn_memBlockDestroy(memBlock); }
    return errResult;
}

void* lvn_memArenaAlloc(LvnMemoryArena* memArena, size_t size)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");

    if (!size)
        return NULL;

    void* ptr = NULL;

    // get alloc from first block in arena if available
    uint8_t* alignedIndex = (uint8_t*) LVN_ALIGN_UP((uintptr_t)memArena->blocks->currIndex, memArena->align);
    if ((alignedIndex + size) <= (memArena->blocks->allocAligned + memArena->blocks->size))
    {
        ptr = alignedIndex;
        memArena->blocks->currIndex = alignedIndex + size;
        goto alloc_success;
    }

    // create new memory block if no space left
    size_t newSize = lvn_memArenaGetTotalSize(memArena);
    newSize = (newSize < size) ? size * 2 : newSize;

    if (lvn_memArenaPushBlock(memArena, newSize) != Lvn_Result_Success)
        return NULL;

    alignedIndex = (uint8_t*) LVN_ALIGN_UP((uintptr_t)memArena->blocks->currIndex, memArena->align);
    if ((alignedIndex + size) <= (memArena->blocks->allocAligned + memArena->blocks->size))
    {
        ptr = alignedIndex;
        memArena->blocks->currIndex = alignedIndex + size;
        goto alloc_success;
    }

    // unable to find next block index
    return NULL;

alloc_success:
    memArena->allocCount++;
    return ptr;
}

void* lvn_memArenaAllocAligned(LvnMemoryArena* memArena, size_t size, size_t align)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");
    LVN_CMA_ASSERT(align != 0 && (align & (align - 1)) == 0, "align cannot be zero or a non power of two");

    if (!size)
        return NULL;

    void* ptr = NULL;

    // get alloc from first block in arena if available
    uint8_t* alignedIndex = (uint8_t*) LVN_ALIGN_UP((uintptr_t)memArena->blocks->currIndex, align);
    if ((alignedIndex + size) <= (memArena->blocks->allocAligned + memArena->blocks->size))
    {
        ptr = alignedIndex;
        memArena->blocks->currIndex = alignedIndex + size;
        goto alloc_success;
    }

    // create new memory block if no space left
    size_t newSize = lvn_memArenaGetTotalSize(memArena);
    newSize = (newSize < size) ? size * 2 : newSize;

    if (lvn_memArenaPushBlock(memArena, newSize) != Lvn_Result_Success)
        return NULL;

    alignedIndex = (uint8_t*) LVN_ALIGN_UP((uintptr_t)memArena->blocks->currIndex, align);
    if ((alignedIndex + size) <= (memArena->blocks->allocAligned + memArena->blocks->size))
    {
        ptr = alignedIndex;
        memArena->blocks->currIndex = alignedIndex + size;
        goto alloc_success;
    }

    // unable to find next block index
    return NULL;

alloc_success:
    memArena->allocCount++;
    return ptr;
}

LvnArenaMark lvn_memArenaMark(LvnMemoryArena* memArena)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");
    return (LvnArenaMark){
        .block = memArena->blocks,
        .offset = (uintptr_t)(memArena->blocks->currIndex - memArena->blocks->allocAligned),
        .next = NULL,
    };
}

void lvn_memArenaMarkRevert(LvnMemoryArena* memArena, const LvnArenaMark* mark)
{
    LVN_CMA_ASSERT(memArena && mark, "memArena and mark cannot be null");
    LVN_CMA_ASSERT(mark->offset <= mark->block->size, "mark offset must be <= block size");

    LvnMemoryBlock* currBlock = memArena->blocks;
    while (currBlock != mark->block)
    {
        LvnMemoryBlock* temp = currBlock;
        currBlock = currBlock->next;
        lvn_memBlockDestroy(temp);
    }

    memArena->blocks = mark->block;
    memArena->blocks->currIndex = memArena->blocks->allocAligned + mark->offset;
}

void lvn_memArenaMarkPush(LvnMemoryArena* memArena)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");

    LvnArenaMark* mark = NULL;

    mark = (LvnArenaMark*) LVN_CMA_MALLOC(sizeof(LvnArenaMark));
    if (!mark)
        return;

    mark->block = memArena->blocks;
    mark->offset = (uintptr_t)(memArena->blocks->currIndex - memArena->blocks->allocAligned);
    mark->next = memArena->marks;

    memArena->marks = mark;
}

void lvn_memArenaMarkPop(LvnMemoryArena* memArena)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");
    LVN_CMA_ASSERT(memArena->marks, "memArena->marks cannot be null/empty");

    LvnArenaMark* mark = memArena->marks;

    lvn_memArenaMarkRevert(memArena, mark);

    memArena->marks = memArena->marks->next;

    LVN_CMA_FREE(mark);
}

LvnResult lvn_memArenaResetMergeBlocks(LvnMemoryArena* memArena)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");

    LvnResult errResult = Lvn_Result_Failure;
    LvnMemoryBlock* memBlock = NULL;

    size_t totalSize = 0;
    for (LvnMemoryBlock* currBlock = memArena->blocks; currBlock; currBlock = currBlock->next)
        totalSize += currBlock->size;

    LvnMemoryBlockCreateInfo memBlockCreateInfo = {
        .size = totalSize,
        .align = memArena->align,
        .next = NULL,
    };

    LvnResult result = lvn_memBlockCreate(&memBlock, &memBlockCreateInfo);
    if (result != Lvn_Result_Success)
    {
        errResult = result;
        goto fail_cleanup;
    }

    LvnMemoryBlock* currBlock = memArena->blocks;
    while (currBlock)
    {
        LvnMemoryBlock* temp = currBlock;
        currBlock = currBlock->next;
        LVN_CMA_FREE(temp->allocation);
        LVN_CMA_FREE(temp);
    }

    LvnArenaMark* currMark = memArena->marks;
    while (currMark)
    {
        LvnArenaMark* temp = currMark;
        currMark = currMark->next;
        LVN_CMA_FREE(temp);
    }

    memArena->blocks = memBlock;

    return Lvn_Result_Success;

fail_cleanup:
    if (memBlock) { lvn_memBlockDestroy(memBlock); }
    return errResult;
}

size_t lvn_memArenaGetTotalSize(LvnMemoryArena* memArena)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");

    size_t size = 0;

    for (LvnMemoryBlock* currBlock = memArena->blocks; currBlock; currBlock = currBlock->next)
        size += currBlock->size;

    return size;
}

size_t lvn_memArenaGetAllocCount(LvnMemoryArena* memArena)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");
    return memArena->allocCount;
}

#endif // LVN_CMA_IMPL


#endif // !LVN_CUSTOM_MEMORY_ALLOCATOR_H
