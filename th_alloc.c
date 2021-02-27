/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

/* Tar Heels Allocator
 *
 * Simple Hoard-style malloc/free implementation.
 * Not suitable for use for large allocatoins, or
 * in multi-threaded programs.
 *
 * to use:
 * $ export LD_PRELOAD=/path/to/th_alloc.so <your command>
 */

/* Hard-code some system parameters */

#define SUPER_BLOCK_SIZE 4096
#define SUPER_BLOCK_MASK (~(SUPER_BLOCK_SIZE - 1))
#define MIN_ALLOC 32   /* Smallest real allocation.  Round smaller mallocs up */
#define MAX_ALLOC 2048 /* Fail if anything bigger is attempted. \
                        * Challenge: handle big allocations */
#define RESERVE_SUPERBLOCK_THRESHOLD 2

#define FREE_POISON 0xab
#define ALLOC_POISON 0xcd

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#define assert(cond) \
    if (!(cond))     \
    __asm__ __volatile__("int $3")

/* Object: One return from malloc/input to free. */
struct __attribute__((packed)) object
{
    union
    {
        struct object *next; // For free list (when not in use)
        char *raw;           // Actual data
    };
};

/* Super block bookeeping; one per superblock.  "steal" the first
 * object to store this structure
 */
struct __attribute__((packed)) superblock_bookkeeping
{
    struct superblock_bookkeeping *next; // next super block
    struct object *free_list;
    // Free count in this superblock
    uint8_t free_count; // Max objects per superblock is 128-1, so a byte is sufficient
    uint8_t level;
};

/* Superblock: a chunk of contiguous virtual memory.
 * Subdivide into allocations of same power-of-two size. */
struct __attribute__((packed)) superblock
{
    struct superblock_bookkeeping bkeep;
    void *raw; // Actual data here
};

/* The structure for one pool of superblocks.
 * One of these per power-of-two */
struct superblock_pool
{
    struct superblock_bookkeeping *next;
    uint64_t free_objects;      // Total number of free objects across all superblocks
    uint64_t whole_superblocks; // Superblocks with all entries free
};

// 10^5 -- 10^11 == 7 levels
#define LEVELS 7
static struct superblock_pool levels[LEVELS] = {{NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0},
                                                {NULL, 0, 0}};
/* Data structure to track large objects (bigger than MAX_ALLOC.
 * We will just do a simple linked-list by default.
 *
 * Extra credit to do something more substantial (or before seeing this code).
 */
struct big_object
{
    struct big_object *next;
    void *addr;
    size_t size;
};

// List of big objects
static struct big_object *big_object_list = NULL;

/* Convert the size to the correct power of two.
 * Recall that the 0th entry in levels is really 2^5,
 * the second level represents 2^6, etc.
 *
 * Return the index to the appropriate level (0..6), or
 * -1 if the size is too large.
 */

static inline int size2level(ssize_t size)
{
    /* 
     * Convert the size to the correct power of two.
     * Recall that the 0th entry in levels is really 2^5,
     * the second level represents 2^6, etc.
     */

    if ((size <= 0) || (size > MAX_ALLOC))
    {
        //too small or too big
        errno = -ENOMEM;
        return -1;
    }
    else
    {
        //level 0, from 1 byte to 2^5
        if ((size > 0) && (size <= 32))
        {
            return 0;
        }
        else if ((size > 32) && (size <= 64))
        {
            return 1;
        }
        else if ((size > 64) && (size <= 128))
        {
            return 2;
        }
        else if ((size > 128) && (size <= 256))
        {
            return 3;
        }
        else if ((size > 256) && (size <= 512))
        {
            return 4;
        }
        else if ((size > 512) && (size <= 1024))
        {
            return 5;
        }
        else
        {
            return 6;
        }
    }
    return 0; //never returns
}

/* This function allocates and initializes a new superblock.
 *
 * Note that a superblock in this lab is only one 4KiB page, not
 * 8 KiB, as in the hoard paper.
 *
 * This design sacrifices the first entry in every superblock
 * to store a superblock_bookkeeping structure.  Yes,
 * it is a bit wasteful, but let's keep the exercise simple.
 *
 * power: the power of two to store in this superblock.  Note that
 *        this is offset by 5; so a power zero means 2^5.
 *
 * Return value: a struct superblock_bookkeeping, which is
 * embedded at the start of the superblock.  Or NULL on failure.
 */
static inline struct superblock_bookkeeping *alloc_super(int power)
{

    void *page;
    struct superblock *sb;
    int free_objects = 0, bytes_per_object = 0;
    char *cursor;

    // Allocate a page of anonymous memory
    //mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
    page = mmap(NULL, SUPER_BLOCK_SIZE, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (page == MAP_FAILED)
    {
        //Page allocation not successful --page 3 of linux mn -errno
        errno = -ENOMEM;
        exit(errno);
    }

    sb = (struct superblock *)page;
    // Put this one the list.
    sb->bkeep.next = levels[power].next;
    levels[power].next = &sb->bkeep;
    levels[power].whole_superblocks++;
    sb->bkeep.level = power;
    sb->bkeep.free_list = NULL;

    // Calculate and fill the number of free objects in this superblock
    //  Be sure to add this many objects to levels[power]->free_objects, reserving
    //  the first one for the bookkeeping.
    // Be sure to set free_objects and bytes_per_object to non-zero values.

    //assigning bytes_per_object the real object size
    bytes_per_object = 1 << (power + 5);

    //total size of superblocks divided by the size of the objects, equals the
    // number of objects of that size in that superblock
    free_objects = (SUPER_BLOCK_SIZE / bytes_per_object) - 1;

    //subtract one to reserve the 1st position of the superblock for bookkeeping
    levels[power].free_objects += free_objects;

    //updating free_count of superblock bookkeeping to the number of free objects in superblock
    sb->bkeep.free_count = free_objects;

    // The following loop populates the free list with some atrocious
    // pointer math.  You should not need to change this, provided that you
    // correctly calculate free_objects.

    cursor = (char *)sb;
    // skip the first object
    for (cursor += bytes_per_object; free_objects--; cursor += bytes_per_object)
    {
        // Place the object on the free list
        struct object *tmp = (struct object *)cursor;
        tmp->next = sb->bkeep.free_list;
        sb->bkeep.free_list = tmp;
    }
    return &sb->bkeep;
}

void *malloc(size_t size)
{
    struct superblock_pool *pool;
    struct superblock_bookkeeping *bkeep;
    void *rv = NULL;
    int power = size2level(size);

    // Handle bigger allocations with mmap, and a simple list
    if (size > MAX_ALLOC)
    {
        // Why, yes we can do a recursive malloc.  But carefully...
        struct big_object *biggun = malloc(sizeof(struct big_object));
        biggun->next = big_object_list;
        biggun->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(biggun->addr);
        biggun->size = size;
        big_object_list = biggun;
        return biggun->addr;
    }

    if (power < 0)
    {
        //invalid power, ask what we should do
        errno = -ENOMEM;
        return NULL;
    }

    pool = &levels[power];

    if (!pool->free_objects)
    {
        bkeep = alloc_super(power);
    }
    else
    {
        bkeep = pool->next;
    }

    for (; bkeep != NULL; bkeep = bkeep->next)
    {
        if (bkeep->free_count)
        {
            struct object *cursor = bkeep->free_list;

            /* Remove an object from the free list. */
            bkeep->free_list = cursor->next;

            //set next value of current object to NULL
            cursor->next = NULL;

            // NB: If you take the first object out of a whole
            //     superblock, decrement levels[power]->whole_superblocks
            int max_free_count = (SUPER_BLOCK_SIZE / (1 << (bkeep->level + 5))) - 1;
            if (bkeep->free_count == max_free_count)
            {
                pool->whole_superblocks -= 1;
            }

            //update free_object count in the current pool and free_count in the current superblock
            pool->free_objects -= 1;
            bkeep->free_count -= 1;

            //update rv to the first free object available
            rv = cursor;

            break;
        }
    }

    // assert that rv doesn't end up being NULL at this point
    assert(rv != NULL);

    /* Exercise 3: Poison a newly allocated object to detect init errors.
     * Hint: use ALLOC_POISON
     */
    memset(rv, ALLOC_POISON, (1 << (power + 5)));

    return rv;
}

static inline struct superblock_bookkeeping *obj2bkeep(void *ptr)
{
    uint64_t addr = (uint64_t)ptr;
    addr &= SUPER_BLOCK_MASK;
    return (struct superblock_bookkeeping *)addr;
}

void free(void *ptr)
{

    // Just ignore free of a null ptr
    if (ptr == NULL)
        return;

    struct superblock_bookkeeping *bkeep = obj2bkeep(ptr);
    int power = bkeep->level;

    // We need to check for free of any large objects first.
    {
        struct big_object *tmp, *last;
        for (tmp = big_object_list, last = NULL; tmp; last = tmp, tmp = tmp->next)
        {
            if (tmp->addr == ptr)
            {
                // We found it
                // Unmap the object
                munmap(tmp->addr, tmp->size);
                // Fix up the list
                if (!last)
                {
                    big_object_list = tmp->next;
                }
                else
                {
                    last->next = tmp->next;
                }
                // Free the node
                free(tmp);
                return;
            }
        }
    }

    // Place the object on the free list
    struct object *obj_ptr = (struct object *)ptr;
    memset(ptr, FREE_POISON, (1 << (power + 5)));
    struct object *prev_head = bkeep->free_list;
    bkeep->free_list = obj_ptr;
    bkeep->free_list->next = prev_head;

    //Update whole_superblocks if needed
    int max_free_count = (SUPER_BLOCK_SIZE / (1 << (power + 5))) - 1;
    if (bkeep->free_count == max_free_count - 1)
    {
        levels[power].whole_superblocks += 1;
    }

    //Update free_object count in the current pool and free_count in the current superblock
    levels[power].free_objects += 1;
    bkeep->free_count += 1;

    struct superblock_bookkeeping *temp_bkeep;
    struct superblock_bookkeeping *prev_bkeep;
    struct superblock_pool *pool;
    int moving_pool_head = 1;

    pool = &levels[power];
    temp_bkeep = pool->next;
    prev_bkeep = temp_bkeep;

    while (levels[power].whole_superblocks > RESERVE_SUPERBLOCK_THRESHOLD)
    {
        // Remove a whole superblock from the level
        // Return that superblock to the OS, using mmunmap

        if (temp_bkeep->free_count == max_free_count) //if superblock is completely empty, return superblock to the OS
        {

            if (moving_pool_head)
            {
                //updating superblock pool head
                pool->next = temp_bkeep->next;

                //returning superblock to the OS
                munmap(temp_bkeep, SUPER_BLOCK_SIZE);

                //updating bookkeeping pointers
                temp_bkeep = pool->next;
                prev_bkeep = temp_bkeep;
            }
            else
            {
                //updating links in superblock bookkeeping list in current pool
                prev_bkeep->next = temp_bkeep->next;
                prev_bkeep = temp_bkeep->next;

                //returning superblock to the OS
                munmap(temp_bkeep, SUPER_BLOCK_SIZE);

                //updating bookkeeping pointers
                temp_bkeep = prev_bkeep;
            }

            //update count of total free objects in current superblock pool
            pool->free_objects -= max_free_count;

            //update number of whole superblocks in current superblock pool
            pool->whole_superblocks -= 1;
        }
        else
        {
            //updating bookkeeping pointers
            prev_bkeep = temp_bkeep;

            //moving pointer to next superblock bookkeeping
            temp_bkeep = temp_bkeep->next;

            //setting moving_pool_head to false
            moving_pool_head = 0;
        }
    }
}

// Do NOT touch this - this will catch any attempt to load this into a multi-threaded app
int pthread_create(void __attribute__((unused)) * x, ...)
{
    exit(-ENOSYS);
}
