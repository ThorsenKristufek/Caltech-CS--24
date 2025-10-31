#ifndef WEENSYOS_UTIL_H
#define WEENSYOS_UTIL_H

#include <stdbool.h>
#include "x86-64.h"
#include "types.h"
#include "k-vmiter.hh"

extern "C" {
    typedef bool (*iteration_func_t)(x86_64_pagetable *table, uintptr_t virtual_address);
    void memory_foreach(x86_64_pagetable *table, uintptr_t upper_bound, iteration_func_t iter);  

    int memory_map(x86_64_pagetable *table, uintptr_t virtual_address, uintptr_t physical_address, int permissions);
    void * memory_virtual_to_physical(x86_64_pagetable *table, uintptr_t virtual_address);
    uintptr_t memory_permissions(x86_64_pagetable *table, uintptr_t virtual_address);

    bool map_to_nobody(x86_64_pagetable *table, uintptr_t va);
    bool map_to_kernel_space(x86_64_pagetable *table, uintptr_t va);
    bool map_to_user_space(x86_64_pagetable *table, uintptr_t va);
}
#endif
