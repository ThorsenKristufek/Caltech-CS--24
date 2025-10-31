/*
 * mm-explicit.c - The best malloc package EVAR!
 *
 * TODO (bug): Uh..this is an implicit list???
 */

#include <stdint.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"

/** The required alignment of heap payloads */
const size_t ALIGNMENT = 2 * sizeof(size_t);

/** The layout of each block allocated on the heap */
typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    /**
     * We don't know what the size of the payload will be, so we will
     * declare it as a zero-length array.  This allow us to obtain a
     * pointer to the start of the payload.
     */
    uint8_t payload[];
} block_t;

typedef struct {
    /** The size of the block and whether it is allocated (stored in the low bit) */
    size_t header;
    block_t *prev;
    block_t *next;
} link_free_blocks_t;

typedef struct {
    size_t size;
} foot_t;

/** The first and last blocks on the heap */
static link_free_blocks_t *head = NULL;
static block_t *mm_heap_first = NULL;
static block_t *mm_heap_last = NULL;

// Helper function to insert a block into the free list
static void insert_into_free_list(block_t *block) {
    link_free_blocks_t *free_block = (link_free_blocks_t *) block;
    free_block->next = (block_t *) head;
    if (head != NULL) {
        head->prev = (block_t *) free_block;
    }
    free_block->prev = NULL;
    head = free_block;
}

// Helper function to remove a block from the free list
static void remove_from_free_list(block_t *block) {
    link_free_blocks_t *free_block = (link_free_blocks_t *) block;
    if (free_block->prev != NULL) {
        ((link_free_blocks_t *) (free_block->prev))->next = free_block->next;
    }
    else {
        head = (link_free_blocks_t *) (free_block->next);
    }
    if (free_block->next != NULL) {
        ((link_free_blocks_t *) (free_block->next))->prev = free_block->prev;
    }
}

/** Rounds up `size` to the nearest multiple of `n` */
static size_t round_up(size_t size, size_t n) {
    return (size + (n - 1)) / n * n;
}

/** Set's a block's header with the given size and allocation state */
static void set_header(block_t *block, size_t size, bool is_allocated) {
    block->header = size | is_allocated;
    foot_t *footer = (void *) block + size - sizeof(foot_t);
    footer->size = size;
}

/** Extracts a block's size from its header */
static size_t get_size(block_t *block) {
    return block->header & ~1;
}

/** Extracts a block's allocation state from its header */
static bool is_allocated(block_t *block) {
    return block->header & 1;
}

/**
 * Finds the first free block in the heap with at least the given size.
 * If no block is large enough, returns NULL.
 */
static block_t *find_fit(size_t size) {
    // Traverse the blocks in the heap using the implicit list
    for (link_free_blocks_t *curr = head; curr != NULL;
         curr = (link_free_blocks_t *) curr->next) {
        // If the block is free and large enough for the allocation, return it
        block_t *iter = (block_t *) curr;
        if (get_size(iter) >= size) {
            return iter;
        }
    }
    return NULL;
}

/** Gets the header corresponding to a given payload pointer */
static block_t *block_from_payload(void *ptr) {
    return ptr - offsetof(block_t, payload);
}

/**
 * mm_init - Initializes the allocator state
 */
bool mm_init(void) {
    // We want the first payload to start at ALIGNMENT bytes from the start of the heap
    void *padding = mem_sbrk(ALIGNMENT - sizeof(block_t));
    if (padding == (void *) -1) {
        return false;
    }
    head = NULL;
    // Initialize the heap with no blocks
    mm_heap_first = NULL;
    mm_heap_last = NULL;
    return true;
}

// Helper Function to Coalesce in Free
void coalesce(block_t *block) {
    block_t *prev = NULL;
    block_t *next = NULL;
    if (block != mm_heap_first) {
        foot_t *prev_footer = (void *) block - sizeof(foot_t);
        prev = (void *) block - prev_footer->size;
    }
    if (block != mm_heap_last) {
        next = (void *) block + get_size(block);
    }

    if (prev != NULL && next != NULL && !is_allocated(prev) && !is_allocated(next)) {
        set_header(prev, get_size(block) + get_size(prev) + get_size(next), false);
        remove_from_free_list(next);
        remove_from_free_list(block);
        if (next == mm_heap_last) {
            mm_heap_last = prev;
        }
    }
    else if (next != NULL && !is_allocated(next)) {
        set_header(block, get_size(block) + get_size(next), false);
        remove_from_free_list(next);
        if (next == mm_heap_last) {
            mm_heap_last = block;
        }
    }
    else if (prev != NULL && !is_allocated(prev)) {
        set_header(prev, get_size(block) + get_size(prev), false);
        remove_from_free_list(block);
        if (block == mm_heap_last) {
            mm_heap_last = prev;
        }
    }
    else {
        set_header(block, get_size(block), false);
    }
}

/**
 * mm_malloc - Allocates a block with the given size
 */
void *mm_malloc(size_t size) {
    // The block must have enough space for a header and be 16-byte aligned
    size = round_up(sizeof(block_t) + size + sizeof(foot_t), ALIGNMENT);
    // If there is a free block that is big enough, should use it
    block_t *block = find_fit(size);
    if (block != NULL) {
        remove_from_free_list(block);
        block_t *end = (void *) block + size;
        size_t block_size = get_size(block);
        if (block_size > size + sizeof(block_t) + sizeof(foot_t)) {
            if (block == mm_heap_last) {
                mm_heap_last = end;
            }
            set_header(end, block_size - size, false);
            insert_into_free_list(end);
            block_size = size;
        }
        set_header(block, block_size, true);

        return block->payload;
    }

    // Otherwise, a new block needs to be allocated at the end of the heap
    block = mem_sbrk(size);
    if (block == (void *) -1) {
        return NULL;
    }
    // Update mm_heap_first and mm_heap_last since we extended the heap
    if (mm_heap_first == NULL) {
        mm_heap_first = block;
    }
    mm_heap_last = block;
    // Initialize the block with the allocated size
    set_header(block, size, true);
    return block->payload;
}

/**
 * mm_free - Releases a block to be reused for future allocations
 */
void mm_free(void *ptr) {
    // mm_free(NULL) does nothing
    if (ptr == NULL) {
        return;
    }

    // Mark the block as unallocated and insert it into the free list
    block_t *block = block_from_payload(ptr);
    set_header(block, get_size(block), false);
    insert_into_free_list(block);

    // Coalesce if possible
    coalesce(block);
}

/**
 * mm_realloc - Change the size of the block by mm_mallocing a new block,
 *      copying its data, and mm_freeing the old block.
 */
void *mm_realloc(void *old_ptr, size_t size) {
    if (size == 0) {
        mm_free(old_ptr);
        return NULL;
    }
    if (old_ptr == NULL) {
        return mm_malloc(size);
    }
    block_t *old_block = block_from_payload(old_ptr);
    size_t old_size = get_size(old_block) - sizeof(block_t);
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }
    size_t copy_size = size < old_size ? size : old_size;
    memcpy(new_ptr, old_ptr, copy_size);
    mm_free(old_ptr);
    return new_ptr;
}

/**
 * mm_calloc - Allocate the block and set it to zero.
 */
void *mm_calloc(size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    block_t *block = mm_malloc(bytes);
    memset(block, 0, bytes);
    return block;
}

/**
 * mm_checkheap - So simple, it doesn't need a checker!
 */
void mm_checkheap(void) {
}
