mm_realloc
- The mm realloc routine returns a pointer to an allocated region of at least size bytes with the following constraints.
- if ptr is NULL, the call is equivalent to mm_malloc(size);
- if size is equal to zero, the call is equivalent to mm_free(ptr);
- if ptr is not NULL, it must have been returned by an earlier call to mm_malloc or mm_realloc. </br>
  The call to mm realloc changes the size of the memory block pointed to by ptr (the old block) to size bytes and </br>
  returns the address of the new block. Notice that the address of the new block might be the same as the old block, </br>
  or it might be different, depending on your imple- mentation, the amount of internal fragmentation in the old block, </br>
  and the size of the realloc request. The contents of the new block are the same as those of the old ptr block, up to the minimum of the old and new sizes. Everything else is uninitialized. For example, if the old block is 8 bytes and the new block is 12 bytes, then the first 8 bytes of the new block are identical to the first 8

These semantics match the the semantics of the corresponding libc malloc, realloc, and free rou- tines. Type man malloc to the shell for complete documentation.