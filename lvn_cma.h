#ifndef HG_LVN_CMA_H
#define HG_LVN_CMA_H

// debug
#ifndef LVN_CONFIG_DEBUG
    #ifndef NDEBUG
        #define LVN_CONFIG_DEBUG
    #endif
#endif

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

// alignment
#if defined(__cplusplus)
    #define LVN_ALIGNOF(T) alignof(T)
#elif defined(__STDC__) && (__STDC_VERSION__ >= 201112L)
    #define LVN_ALIGNOF(T) _Alignof(T)
#else
    #define LVN_ALIGNOF(T) ((size_t)offsetof(struct { char c; T x; }, x))
#endif

#define LVN_DEFAULT_ALIGN (sizeof(void*))
#define LVN_ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define LVN_ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define LVN_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

// memory growth mutipler
#ifndef LVN_MEMPOOL_NEXT_MALLOC_MULTIPLIER
    #define LVN_MEMPOOL_NEXT_MALLOC_MULTIPLIER (2)
#endif
#ifndef LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER
    #define LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER (2)
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


typedef struct LvnFreeNode
{
    struct LvnFreeNode* next;
} LvnFreeNode;

typedef struct LvnMemoryBlock
{
    void* block;                   /* the actual pointer to the allocation */
    uint8_t* blockAligned;         /* aligned pointer offset within the block allocation, alignment needs to be specified */
    size_t size;                   /* size of the allocation in bytes */
} LvnMemoryBlock;

typedef struct LvnMemoryPool
{
    LvnMemoryBlock memBlock;       /* the memory block */
    size_t currIndex;              /* current index to allocate the next element from the memory block */
    size_t capacity;               /* the max count of elements within the pool (not to be confused with size of block allocation in bytes) */
    size_t stride;                 /* the stride of the element in bytes in the pool (requested size) */
    size_t align;                  /* the alignment multiple of the elements in bytes */
    size_t strideAligned;          /* the stride aligned to a multiple of align (actual size alloced by pool) */
    LvnFreeNode* freeList;         /* node list of free memory indices in the pool */
    struct LvnMemoryPool* next;    /* next memory pool */

#ifdef LVN_CONFIG_DEBUG
    size_t d_AllocCount;            /* track allocations allocced from pool for debugging */
#endif
} LvnMemoryPool;

typedef struct LvnMemoryArena
{
    LvnMemoryBlock memBlock;        /* the memory block */
    size_t currIndex;               /* current index to allocate the next element from the memory block */
    size_t capacity;                /* the capacity of the user specified memory allocation in bytes (capacity may be different from block allocation size due to alignment) */
    size_t align;                   /* the alignment multiple of the allocation in bytes */
    size_t markIndex;               /* mark of alloc index, used for stack style allocs */
    struct LvnMemoryArena* next;    /* next memory arena */
    bool mark;                      /* memory arena is marked or not */

#ifdef LVN_CONFIG_DEBUG
    size_t d_AllocCount;            /* track allocations allocced from pool for debugging */
#endif
} LvnMemoryArena;


LvnMemoryPool*     lvn_memPoolCreate(size_t count, size_t stride, size_t align);
LvnMemoryPool*     lvn_memPoolPush(LvnMemoryPool* headPool, size_t count);
void               lvn_memPoolDestroy(LvnMemoryPool* headPool);
void*              lvn_memPoolAlloc(LvnMemoryPool* memPool);
void               lvn_memPoolFree(LvnMemoryPool* memPool, void* ptr);
void               lvn_memPoolReset(LvnMemoryPool* headPool);
LvnMemoryPool*     lvn_memPoolRebuild(LvnMemoryPool* headPool);
LvnMemoryArena*    lvn_memArenaCreate(size_t size, size_t align);
LvnMemoryArena*    lvn_memArenaPush(LvnMemoryArena* headArena, size_t size);
void               lvn_memArenaDestroy(LvnMemoryArena* headArena);
void*              lvn_memArenaAlloc(LvnMemoryArena* memArena, size_t size);
void*              lvn_memArenaAllocAligned(LvnMemoryArena* memArena, size_t size, size_t align);
void*              lvn_memArenaAllocMark(LvnMemoryArena* memArena, size_t size);
void*              lvn_memArenaAllocAlignedMark(LvnMemoryArena* memArena, size_t size, size_t align);
void               lvn_memArenaAllocPop(LvnMemoryArena* memArena);
void               lvn_memArenaReset(LvnMemoryArena* headArena);
LvnMemoryArena*    lvn_memArenaRebuild(LvnMemoryArena* headArena);
bool               lvn_memArenaMarked(LvnMemoryArena* headArena);


#ifdef LVN_CMA_IMPL

#include <stdlib.h>
#include <string.h>
#include <assert.h>


bool lvn_ptrInBlock(uint8_t* block, size_t size, void* ptr)
{
    uint8_t* start = block;
    uint8_t* end = start + size;
    return (uint8_t*)ptr >= start && (uint8_t*)ptr < end;
}

LvnMemoryPool* lvn_memPoolCreate(size_t count, size_t stride, size_t align)
{
    LVN_CMA_ASSERT(stride == 0 || count <= SIZE_MAX / stride, "overflow on creating memory size (count * stride)");
    LVN_CMA_ASSERT(align != 0 && (align & (align - 1)) == 0, "align cannot be zero or a non power of two");
    LVN_CMA_ASSERT(align >= LVN_ALIGNOF(void*), "align must be >= pointer alignment");

    LvnMemoryPool* memPool = (LvnMemoryPool*) LVN_CMA_MALLOC(sizeof(LvnMemoryPool));
    if (!memPool) { return NULL; }

    // set min stride
    const size_t minStride = sizeof(void*);
    stride = stride < minStride ? minStride : stride;

    // align stride
    size_t strideAligned = LVN_ALIGN_UP(stride, align);

    LVN_CMA_ASSERT(strideAligned % align == 0, "stride must be multiple of align");

    size_t memSize = count * strideAligned;
    memPool->memBlock.block = LVN_CMA_MALLOC(memSize + align);
    if (!memPool->memBlock.block)
    {
        LVN_CMA_FREE(memPool);
        return NULL;
    }

#ifdef LVN_CONFIG_DEBUG
    memset(memPool->memBlock.block, LVN_DEBUG_ALLOC_VALUE, memSize + align);
    memPool->d_AllocCount = 0;
#endif

    memPool->memBlock.blockAligned = (uint8_t*)LVN_ALIGN_UP((uintptr_t)memPool->memBlock.block, align);
    memPool->memBlock.size = memSize + align;

    memPool->currIndex = 0;
    memPool->capacity = count;
    memPool->stride = stride;
    memPool->align = align;
    memPool->strideAligned = strideAligned;
    memPool->freeList = NULL;
    memPool->next = NULL;

    return memPool;
}

LvnMemoryPool* lvn_memPoolPush(LvnMemoryPool* headPool, size_t count)
{
    LVN_CMA_ASSERT(headPool, "headPool cannot be null");

    LvnMemoryPool* memPool = lvn_memPoolCreate(count, headPool->stride, headPool->align);
    if (!memPool) { return NULL; }

    while (headPool->next)
        headPool = headPool->next;

    headPool->next = memPool;
    return memPool;
}

void lvn_memPoolDestroy(LvnMemoryPool* headPool)
{
    while (headPool)
    {
        LvnMemoryPool* temp = headPool;
        headPool = headPool->next;
        LVN_CMA_FREE(temp->memBlock.block);
        LVN_CMA_FREE(temp);
    }
}

void* lvn_memPoolAlloc(LvnMemoryPool* memPool)
{
    void* ptr = NULL;

    while (memPool)
    {
        // check free list
        if (memPool->freeList)
        {
            ptr = memPool->freeList;
            memPool->freeList = memPool->freeList->next;
            goto alloc_success;
        }

        // get pool next memory block index
        if (memPool->currIndex < memPool->capacity)
        {
            ptr = &memPool->memBlock.blockAligned[memPool->currIndex++ * memPool->strideAligned];
            goto alloc_success;
        }

        // create next pool if possible
        if (!memPool->next)
        {
            LVN_CMA_ASSERT(LVN_MEMPOOL_NEXT_MALLOC_MULTIPLIER != 0, "memory pool multiplier cannot be zero");
            memPool->next = lvn_memPoolCreate(memPool->capacity * LVN_MEMPOOL_NEXT_MALLOC_MULTIPLIER,
                                              memPool->stride,
                                              memPool->align);
            if (!memPool->next) { return NULL; }
        }

        // pool is full, alloc in next pool
        memPool = memPool->next;
    }

    // unable to find next block index
    return NULL;

alloc_success:
#ifdef LVN_CONFIG_DEBUG
    memPool->d_AllocCount++;
#endif
    return ptr;
}

void lvn_memPoolFree(LvnMemoryPool* memPool, void* ptr)
{
    LVN_CMA_ASSERT(memPool && ptr, "memPool and ptr cannot be null");

    // find which pool ptr was allocated from
    // NOTE: only checks if ptr is within capacity * strideAligned of the block rather than the full block size,
    //       this is to ensure the ptr is a valid ptr that came from pool alloc
    while (memPool && !lvn_ptrInBlock(memPool->memBlock.blockAligned, memPool->capacity * memPool->strideAligned, ptr))
        memPool = memPool->next;
    LVN_CMA_ASSERT(memPool, "ptr not found within pool chain memory boundaries");

    // check alignment of pointer
    size_t offset = (uint8_t*)ptr - (uint8_t*)memPool->memBlock.blockAligned;
    LVN_CMA_ASSERT(offset % memPool->strideAligned == 0, "invalid pool pointer, pointer not aligned to pool stride align");

#ifdef LVN_CONFIG_DEBUG
    // checks if ptr was already freed (double free)
    for (LvnFreeNode* node = memPool->freeList; node; node = node->next)
    {
        LVN_CMA_ASSERT(node != ptr, "double free in memory pool");
    }
    // check if ptr is trying to be freed after pool reset/create
    LVN_CMA_ASSERT(memPool->d_AllocCount > 0, "cannot free ptr to pool, pool was probably reset or just created");
    memPool->d_AllocCount--;
#endif

    // free ptr, add to free list
    LvnFreeNode* node = (LvnFreeNode*) ptr;
    node->next = memPool->freeList;
    memPool->freeList = node;
}

void lvn_memPoolReset(LvnMemoryPool* headPool)
{
    LVN_CMA_ASSERT(headPool, "headPool cannot be null");

    for (LvnMemoryPool* memPool = headPool; memPool; memPool = memPool->next)
    {
        memPool->currIndex = 0;
        memPool->freeList = NULL;
#ifdef LVN_CONFIG_DEBUG
        memset(memPool->memBlock.blockAligned, LVN_DEBUG_FREE_VALUE, memPool->capacity * memPool->strideAligned);
        memPool->d_AllocCount = 0;
#endif
    }
}

LvnMemoryPool* lvn_memPoolRebuild(LvnMemoryPool* headPool)
{
    LVN_CMA_ASSERT(headPool, "headPool cannot be null");

    // clear values only if there is only one pool in the chain (head pool)
    if (!headPool->next)
    {
        lvn_memPoolReset(headPool);
        return headPool;
    }

    size_t totalSize = 0;
    LvnMemoryPool* head = headPool;
    while (head)
    {
        totalSize += head->capacity;
        head = head->next;
    }

    LvnMemoryPool* memPool = lvn_memPoolCreate(totalSize, headPool->stride, headPool->align);
    if (!memPool) { return NULL; }

    head = headPool;
    while (head)
    {
        LvnMemoryPool* temp = head;
        head = head->next;
        LVN_CMA_FREE(temp->memBlock.block);
        LVN_CMA_FREE(temp);
    }

    return memPool;
}

LvnMemoryArena* lvn_memArenaCreate(size_t size, size_t align)
{
    LVN_CMA_ASSERT(size <= SIZE_MAX, "arena size overflow");
    LVN_CMA_ASSERT(align != 0 && (align & (align - 1)) == 0, "align cannot be zero or a non power of two");
    LVN_CMA_ASSERT(align >= LVN_ALIGNOF(void*), "align must be >= pointer alignment");

    LvnMemoryArena* memArena = (LvnMemoryArena*) LVN_CMA_MALLOC(sizeof(LvnMemoryArena));
    if (!memArena) { return NULL; }

    memArena->memBlock.block = LVN_CMA_MALLOC(size + align);
    if (!memArena->memBlock.block)
    {
        LVN_CMA_FREE(memArena);
        return NULL;
    }

#ifdef LVN_CONFIG_DEBUG
    memset(memArena->memBlock.block, LVN_DEBUG_ALLOC_VALUE, size);
    memArena->d_AllocCount = 0;
#endif

    memArena->memBlock.blockAligned = (uint8_t*)LVN_ALIGN_UP((uintptr_t)memArena->memBlock.block, align);
    memArena->memBlock.size = size + align;

    memArena->currIndex = 0;
    memArena->capacity = size;
    memArena->align = align;
    memArena->next = NULL;

    return memArena;
}

LvnMemoryArena* lvn_memArenaPush(LvnMemoryArena* headArena, size_t size)
{
    LVN_CMA_ASSERT(headArena, "headArena cannot be null");

    LvnMemoryArena* memArena = lvn_memArenaCreate(size, headArena->align);
    if (!memArena) { return NULL; }

    while (headArena->next)
        headArena = headArena->next;

    headArena->next = memArena;
    return memArena;
}

void lvn_memArenaDestroy(LvnMemoryArena* headArena)
{
    while (headArena)
    {
        LvnMemoryArena* temp = headArena;
        headArena = headArena->next;
        LVN_CMA_FREE(temp->memBlock.block);
        LVN_CMA_FREE(temp);
    }
}

void* lvn_memArenaAlloc(LvnMemoryArena* memArena, size_t size)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");

    while (memArena)
    {
        // get arena next memory block index
        uintptr_t base = (uintptr_t)memArena->memBlock.blockAligned;
        uintptr_t aligned = LVN_ALIGN_UP(base + memArena->currIndex, memArena->align);
        size_t newIndex = aligned - base;
        if (newIndex + size <= memArena->capacity)
        {
            void* ptr = &(memArena->memBlock.blockAligned)[memArena->currIndex];
            memArena->currIndex = newIndex + size;
#ifdef LVN_CONFIG_DEBUG
            memArena->d_AllocCount++;
#endif
            return ptr;
        }

        // create next arena if possible
        if (!memArena->next)
        {
            LVN_CMA_ASSERT(LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER != 0, "memory arena multiplier cannot be zero");
            size_t nextSize = memArena->capacity * LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER;
            nextSize = (size > nextSize) ? size : nextSize;
            memArena->next = lvn_memArenaCreate(nextSize, memArena->align);
            if (!memArena->next) { return NULL; }
        }

        // arena is full, get alloc in next arena
        memArena = memArena->next;
    }

    // unable to find next block index
    return NULL;
}

void* lvn_memArenaAllocAligned(LvnMemoryArena* memArena, size_t size, size_t align)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");
    LVN_CMA_ASSERT(align != 0 && (align & (align - 1)) == 0, "align cannot be zero or a non power of two");

    while (memArena)
    {
        // get arena next memory block index
        uintptr_t base = (uintptr_t)memArena->memBlock.blockAligned;
        uintptr_t aligned = LVN_ALIGN_UP(base + memArena->currIndex, align);
        size_t newIndex = aligned - base;
        if (newIndex + size <= memArena->capacity)
        {
            void* ptr = &(memArena->memBlock.blockAligned)[newIndex];
            memArena->currIndex = newIndex + size;
#ifdef LVN_CONFIG_DEBUG
            memArena->d_AllocCount++;
#endif
            return ptr;
        }

        // create next arena if possible
        if (!memArena->next)
        {
            LVN_CMA_ASSERT(LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER != 0, "memory arena multiplier cannot be zero");
            size_t nextSize = memArena->capacity * LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER;
            nextSize = (size > nextSize) ? size : nextSize;
            memArena->next = lvn_memArenaCreate(nextSize, memArena->align);
            if (!memArena->next) { return NULL; }
        }

        // arena is full, get alloc in next arena
        memArena = memArena->next;
    }

    // unable to find next block index
    return NULL;
}

void* lvn_memArenaAllocMark(LvnMemoryArena* memArena, size_t size)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");
    LVN_CMA_ASSERT(size > 0, "size cannot be zero, marking empty alloc is not allowed");
    LVN_CMA_ASSERT(!lvn_memArenaMarked(memArena), "memArena cannot be marked");

    while (memArena)
    {
        // get arena next memory block index
        uintptr_t base = (uintptr_t)memArena->memBlock.blockAligned;
        uintptr_t aligned = LVN_ALIGN_UP(base + memArena->currIndex, memArena->align);
        size_t newIndex = aligned - base;
        if (newIndex + size <= memArena->capacity)
        {
            void* ptr = &(memArena->memBlock.blockAligned)[memArena->currIndex];
            memArena->markIndex = memArena->currIndex;
            memArena->mark = true;
            memArena->currIndex = newIndex + size;
#ifdef LVN_CONFIG_DEBUG
            memArena->d_AllocCount++;
#endif
            return ptr;
        }

        // create next arena if possible
        if (!memArena->next)
        {
            LVN_CMA_ASSERT(LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER != 0, "memory arena multiplier cannot be zero");
            size_t nextSize = memArena->capacity * LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER;
            nextSize = (size > nextSize) ? size : nextSize;
            memArena->next = lvn_memArenaCreate(nextSize, memArena->align);
            if (!memArena->next) { return NULL; }
        }

        // arena is full, get alloc in next arena
        memArena = memArena->next;
    }

    // unable to find next block index
    return NULL;
}

void* lvn_memArenaAllocAlignedMark(LvnMemoryArena* memArena, size_t size, size_t align)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");
    LVN_CMA_ASSERT(align != 0 && (align & (align - 1)) == 0, "align cannot be zero or a non power of two");

    while (memArena)
    {
        // get arena next memory block index
        uintptr_t base = (uintptr_t)memArena->memBlock.blockAligned;
        uintptr_t aligned = LVN_ALIGN_UP(base + memArena->currIndex, align);
        size_t newIndex = aligned - base;
        if (newIndex + size <= memArena->capacity)
        {
            void* ptr = &(memArena->memBlock.blockAligned)[newIndex];
            memArena->markIndex = memArena->currIndex;
            memArena->mark = true;
            memArena->currIndex = newIndex + size;
#ifdef LVN_CONFIG_DEBUG
            memArena->d_AllocCount++;
#endif
            return ptr;
        }

        // create next arena if possible
        if (!memArena->next)
        {
            LVN_CMA_ASSERT(LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER != 0, "memory arena multiplier cannot be zero");
            size_t nextSize = memArena->capacity * LVN_MEMARENA_NEXT_MALLOC_MULTIPLIER;
            nextSize = (size > nextSize) ? size : nextSize;
            memArena->next = lvn_memArenaCreate(nextSize, memArena->align);
            if (!memArena->next) { return NULL; }
        }

        // arena is full, get alloc in next arena
        memArena = memArena->next;
    }

    // unable to find next block index
    return NULL;
}

void lvn_memArenaAllocPop(LvnMemoryArena* memArena)
{
    LVN_CMA_ASSERT(memArena, "memArena cannot be null");

    for (LvnMemoryArena* arena = memArena; arena; arena = arena->next)
    {
        if (arena->mark)
        {
            arena->mark = false;
            arena->currIndex = arena->markIndex;
            return;
        }
    }

    LVN_CMA_ASSERT(false, "memArena or its children arenas must have a marked arena");
}

void lvn_memArenaReset(LvnMemoryArena* headArena)
{
    LVN_CMA_ASSERT(headArena, "memArena cannot be null");

    for (LvnMemoryArena* memArena = headArena; memArena; memArena = memArena->next)
    {
        memArena->currIndex = 0;
#ifdef LVN_CONFIG_DEBUG
        memset(memArena->memBlock.blockAligned, LVN_DEBUG_FREE_VALUE, memArena->capacity);
        memArena->d_AllocCount = 0;
#endif
    }
}

LvnMemoryArena* lvn_memArenaRebuild(LvnMemoryArena* headArena)
{
    LVN_CMA_ASSERT(headArena, "memArena cannot be null");

    // clear values only if there is only one arena in the chain (head arena)
    if (!headArena->next)
    {
        lvn_memArenaReset(headArena);
        return headArena;
    }

    size_t totalSize = 0;
    LvnMemoryArena* head = headArena;
    while (head)
    {
        totalSize += head->capacity;
        head = head->next;
    }

    LvnMemoryArena* memArena = lvn_memArenaCreate(totalSize, headArena->align);
    if (!memArena) { return NULL; }

    head = headArena;
    while (head)
    {
        LvnMemoryArena* temp = head;
        head = head->next;
        LVN_CMA_FREE(temp->memBlock.block);
        LVN_CMA_FREE(temp);
    }

    return memArena;
}

bool lvn_memArenaMarked(LvnMemoryArena* headArena)
{
    LVN_CMA_ASSERT(headArena, "headArena cannot be null");

    for (LvnMemoryArena* arena = headArena; arena; arena = arena->next) {
        if (arena->mark)
            return true;
    }

    return false;
}

#endif // LVN_CMA_IMPL

#endif // !HG_LVN_CMA_H
