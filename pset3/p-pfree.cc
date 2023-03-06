//p-kill
#include "u-lib.hh"
#ifndef ALLOC_SLOWDOWN
#define ALLOC_SLOWDOWN 100
#endif

extern uint8_t end[];

// These global variables go on the data page.
uint8_t* heap_top;
uint8_t* stack_bottom;

void process_main() {

    pid_t p = sys_getpid();
    srand(p);
    heap_top = (uint8_t*) round_up((uintptr_t) end, PAGESIZE);
    uint8_t* lower_bound = heap_top;
    stack_bottom = (uint8_t*) round_down((uintptr_t) rdrsp() - 1, PAGESIZE);

    while (true) {
        if (rand(0, ALLOC_SLOWDOWN - 1) < p) {
            if (heap_top == stack_bottom
                || sys_page_alloc(heap_top) < 0) {
                break;
            }
            *heap_top = p;               // check we can write to new page
            console[CPOS(24, 79)] = p;   // check we can write to console

            heap_top += PAGESIZE;
            if (heap_top - lower_bound > PAGESIZE * 5) {
                sys_page_free((uintptr_t) lower_bound);
                lower_bound += PAGESIZE;
            }
        }
        sys_yield();
    }

    while (true) {
        sys_yield();
    }
}
