#define M61_DISABLE 1
#include "m61.hh"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cinttypes>
#include <cassert>
#include <iostream>

#include <set>
#include <unordered_map>
#include <cstddef>
#include <vector>
#include <algorithm>

// An internal record of all statistics so far.
m61_statistics mystats = {0, 0, 0, 0, 0, 0, 0, 0};

// The total number of freed pointers.
int nfree;

// The total size of all freed allocations.
int free_size;

// A constant used for the alignment of metadata.
size_t METADATA_SIZE = (size_t) alignof(std::max_align_t);

// The largest integer. Used as a special value to indicate a freed pointer.
size_t LARGEST_INT = (size_t)-1;

// A set containing all active pointers allocated through m61_malloc().
std::set<void*> active_ptr;

// A structure that describes all attributes of a pointer.
struct attributes {
  const char* file;
  long line;
  size_t sz;
};

// A map that maps active pointers allocated by m61_malloc() to its according attributes
std::unordered_map <void*, struct attributes> ptrmap;

// A map that maps file names to another dictionary whose keys are lines and values are total sizes of allocation.
std::unordered_map <const char*, std::unordered_map <long, size_t>> hhmap;


/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    if (METADATA_SIZE+sz <= sz) { //avoid integer overflow
      mystats.nfail++;
      mystats.fail_size += sz;
      return 0;
    }

    size_t* metaptr = (size_t*) base_malloc(2*METADATA_SIZE+ sz); //metaptr points to the begining

    if (metaptr == nullptr) {
      mystats.nfail++;
      mystats.fail_size += sz;
      return 0;
    }

    /* Memory layout

      +----------+ <- metaptr
      |    SZ    | (16 bytes)
      +----------+ <- ptr
      |   DATA   | (sz)
      |          |
      | -------- | <- endptr
      |   0xAB   | (8 bytes of padding with magic#)
      +----------|
    */
    *metaptr = sz;
    uintptr_t ptr = (uintptr_t) metaptr + METADATA_SIZE;
    uintptr_t endptr = ptr + sz;
    memset((void*)endptr, 0xAB, 8);

    mystats.ntotal++;
    mystats.total_size += sz;

    if (mystats.heap_min != 0) {
      if (ptr < mystats.heap_min) {
        mystats.heap_min = ptr;
      }
    }
    else {
      mystats.heap_min = ptr;
    }

    if ((uintptr_t)endptr > mystats.heap_max) {
      mystats.heap_max = (uintptr_t)endptr;
    }

    active_ptr.insert((void*)ptr);              // insert ptr to active_ptr set

    struct attributes info = {file, line, sz};  // document the attributes
    ptrmap.insert({(void*) ptr, info});         // insert ptr and its attributes into ptrmap

    // collect heavy hitter information
    if (hhmap.count(file) > 0) {
      if (hhmap.find(file)->second.count(line) > 0) {       // if file & line both exist
        hhmap.find(file)->second.find(line)->second += sz;  // increment size by `sz`
      }
      else {                                            // if line doesn't exist
        hhmap.find(file)->second.insert({ line, sz });  // insert new key-value pair into lineMap
      }
    } else {                                     // if file & line don't exist
      std::unordered_map<long, size_t> lineMap;  // create a new lineMap
      lineMap.insert({ line, sz });              // insert new pair into lineMap
      hhmap.insert({ file, lineMap });           // insert new pair into hhmap
    }

    return (void*) ptr;
}

/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    if (ptr == 0) {
      return;
    }

    if ((uintptr_t)ptr < mystats.heap_min || (uintptr_t)ptr > mystats.heap_max) {
      // ptr not in heap
      fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not in heap\n", file, line, ptr);
      abort();
    }

    /* Memory layout

      +----------+ <- metaptr
      |    SZ    | (16 bytes)
      +----------+ <- ptr
      |   DATA   | (sz)
      |          |
      | -------- | <- endptr
      |   0xAB   | (8 bytes of padding with magic#)
      +----------|
    */
    size_t* metaptr = (size_t*)((uintptr_t)ptr - METADATA_SIZE); // retrive metaptr
    size_t sz;                                                   // retrive size
    memcpy((void*) &sz, (void*) metaptr, sizeof(size_t));
    char* endptr = (char*) ((uintptr_t) ptr + sz);               // retrive endptr

    if (sz == LARGEST_INT) {
      // ptr double free
      fprintf(stderr, "MEMORY BUG: %s.%ld: invalid free of pointer %p, double free\n", file, line, ptr);
      abort();
    }

    if (active_ptr.count((void*)ptr) == 0) {
      // ptr not in the set of active pointers
      fprintf(stderr, "MEMORY BUG: %s:%ld: invalid free of pointer %p, not allocated\n", file, line, ptr);

      std::set<void*>::iterator setIt = active_ptr.begin();

      // loop through all pointers currently in the active pointer set
      for (unsigned long i = 0; i < active_ptr.size(); i++) {
        struct attributes info = ptrmap.find((void*) *setIt)->second;

        if ((uintptr_t)*setIt < (uintptr_t)ptr && (uintptr_t)*setIt+info.sz > (uintptr_t)ptr) {
          // ptr is inside an allocated region
          size_t inside_sz =  (uintptr_t)ptr - (uintptr_t)*setIt; // find inside size
          fprintf(stderr, "%s:%ld: %p is %lu bytes inside a %lu byte region allocated here\n",
            info.file, info.line, ptr, inside_sz, info.sz);
          }

        setIt++;
      }
      abort();
    }

    bool overwritePadding = false;
    for (int i = 0; i < 8; i++) {
      if ((char) *(endptr + i) != (char) 0xAB) {
        overwritePadding = true;
        break;
      }
    }

    if (overwritePadding) {
      // boundary write errors
      fprintf(stderr, "MEMORY BUG: %s.%ld: detected wild write during free of pointer %p\n", file, line, ptr);
      abort();
    }

    nfree++;
    mystats.nactive = mystats.ntotal - nfree;
    free_size += sz;
    mystats.active_size = mystats.total_size - free_size;

    active_ptr.erase((void*)ptr);   // erase freed ptr from active_ptr set
    ptrmap.erase(ptr);              // erase freed ptr from ptrmap
    *metaptr = LARGEST_INT;         // change metadata to detect double free

    base_free((void*) metaptr);
}

/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. If `sz == 0`,
///    then must return a unique, newly-allocated pointer value. Returned
///    memory should be initialized to zero. The allocation request was at
///    location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, long line) {

    if (nmemb * sz / sz != nmemb) { // avoid overflow
      mystats.nfail++;
      mystats.fail_size += sz;
      return 0;
    }
    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }

    mystats.nactive = mystats.ntotal;
    mystats.active_size = mystats.total_size;

    return ptr;
}


/// m61_get_statistics(stats)
///    Store the current memory statistics in `*stats`.

void m61_get_statistics(m61_statistics* stats) {
    stats->nactive = mystats.nactive;
    stats->active_size = mystats.active_size;
    stats->ntotal = mystats.ntotal;
    stats->total_size = mystats.total_size;
    stats->nfail = mystats.nfail;
    stats->fail_size = mystats.fail_size;
    stats->heap_max = mystats.heap_max;
    stats->heap_min = mystats.heap_min;
}


/// m61_print_statistics()
///    Print the current memory statistics.
void m61_print_statistics() {
    m61_statistics stats;
    m61_get_statistics(&stats);
    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_print_leak_report()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_print_leak_report() {

    std::set<void*>::iterator setIt = active_ptr.begin();

    // loop through all pointers currently in the active pointer set
    for (unsigned long i = 0; i < active_ptr.size(); i++) {

      // print out pointer and its attributes
      struct attributes info = ptrmap.find((void*) *setIt)->second;
      const char* file = info.file;
      long line = info.line;
      size_t sz = info.sz;
      printf("LEAK CHECK: %s:%ld: allocated object %p with size %zu\n", file, line, *setIt, sz);

      setIt++;
    }
}

// Sort function for heavy hitter.
// `greater_than` answers the question: should `info1` be placed before `info2` when sorting?
bool greater_than(struct attributes info1, struct attributes info2) {
  return (info1.sz > info2.sz);
}

/// m61_print_heavy_hitter_report()
///    Print a report of heavily-used allocation locations.

void m61_print_heavy_hitter_report() {

  // A vector containing struct attributes of every allocated pointer.
  std::vector<struct attributes> sortv;

  //Counts for total allocated size.
  size_t totalsz = 0;

  //retrive file, line and size information from hhmap
  for (auto it = hhmap.cbegin(); it != hhmap.cend(); ++it) {
    const char* file = it->first;
    auto lineMap = it->second;

    for (auto lit = lineMap.cbegin(); lit != lineMap.cend(); ++lit) {
      long line = lit->first;
      size_t size = lit->second;
      struct attributes info = { file, line, size }; // initialize attributes struct
      sortv.push_back(info);  // insert the struct into sortv

      totalsz += size; // increment total allocated size
    }
  }
  std::sort(sortv.begin(), sortv.end(), greater_than); // sort vector by size attribute

  // neatly prints out sorted vector elements
  for (auto it = sortv.begin(); it != sortv.end(); ++it) {
    double percentage = it->sz * 100;
    percentage /= totalsz;
    printf("HEAVY HITTER: %s:%ld: %zu bytes (~%.1f%%)\n", it->file, it->line, it->sz, percentage);
    if (percentage < 10) {
      break;
    }
  }
}


/// m61_realloc(ptr, sz, file, line)
///    Reallocate the dynamic memory pointed to by `ptr` to hold at least
///    `sz` bytes, returning a pointer to the new block. If `ptr` is
///    `nullptr`, behaves like `m61_malloc(sz, file, line)`. If `sz` is 0,
///    behaves like `m61_free(ptr, file, line)`. The allocation request
///    was at location `file`:`line`.

void* m61_realloc(void* ptr, size_t sz, const char* file, long line) {

  if (ptr == nullptr) {
    return m61_malloc(sz, file, line);
  }
  if (sz == 0) {
    m61_free(ptr, file, line);
    return 0;
  }

  size_t* metaptr = (size_t*)((uintptr_t)ptr - 16);
  size_t oldsz;
  memcpy((void*)&oldsz, (void*)metaptr, sizeof(size_t));

  size_t* newptr = (size_t*) m61_malloc(sz, file, line);

  memcpy(newptr, ptr, std::min(oldsz, sz)); //avoid undefined behavior if (oldsz > sz)

  return (void*)newptr;
}
