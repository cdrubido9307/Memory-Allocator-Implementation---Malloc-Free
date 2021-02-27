# Malloc/Free: Tar Heel Allocator (THALLOC)

A simple single-threaded implemention of `malloc` and `free`, for small objects that serves as a drop-in replacement for the libc malloc and free implementations. This implementation is based on the paper ``Hoard: A Scalable Memory Allocator for Multithreaded Applications.'' It is not suitable for use for large allocations, or in multi-threaded programs.

## How to test THALLOC

- Clone this github repository
- In Linux, open the terminal shell prompt and go to the folder where you copied the repository
- Run **make**
- Run **LD_PRELOAD=./th_alloc.so ./test**

## Functionality

| Function | Description |
| -------- | ----------- |
| size2level | Converts an allocation size to the correct power of two, and then maps it onto the correct index into the levels table. |
| alloc_super | Allocates and initializes a new superblock (4 KB page, not 8 KB as in the Hoard paper). |
| malloc | Handles bigger allocations with mmap, and a simple list. Removes an object from the free list, sets the next value of current object to NULL, decrements free_object count in the current pool and free_count in the current superblock, and updates the value of rv to the first free object available. Using `ALLOC_POISON`, implements memory poisoning on newly allocated object to detect init errors. |
| free | Checks for free of any large objects first, unmaps the object, fixes up the list, and frees the node. After putting the object-to-free back on the free list, the function updates the free count and increments whole_superblocks if all objects are free. Using `FREE_POISON`, implements memory poisoning on newly freed object to detect use-after-free errors. When more than two superblocks are completely free, it releases them back to the operating system using munmap, and updates the appropriate bookkeeping in the allocator. |
