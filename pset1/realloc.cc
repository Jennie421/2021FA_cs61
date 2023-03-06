
/// m61_realloc(ptr, sz, file, line)
///    Reallocate the dynamic memory pointed to by `ptr` to hold at least
///    `sz` bytes, returning a pointer to the new block. If `ptr` is
///    `nullptr`, behaves like `m61_malloc(sz, file, line)`. If `sz` is 0,
///    behaves like `m61_free(ptr, file, line)`. The allocation request
///    was at location `file`:`line`.

void* m61_realloc(void* ptr, size_t sz, const char* file, long line) {

  if (ptr == nulptr) {
    return m61_malloc(sz, file, line);
  }
  if (sz == 0) {
    return m61_free(ptr, file, line);
  }

  size_t* metaptr = (sizet*)((uintptr_t)ptr - 16);
  size_t oldsz;
  memcpy((void*)&oldsz, (void*)metaptr, sizeof(size_t));

  size_t* newptr = (size_t*) m61_malloc(sz, file, line);

  memcpy(newptr, ptr, min(oldsz, sz)); //avoid undefined behavior if (oldsz > sz)

  return (void*)newptr;
}
