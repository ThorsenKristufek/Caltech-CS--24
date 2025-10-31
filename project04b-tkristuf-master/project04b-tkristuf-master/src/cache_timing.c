#include "cache_timing.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

const size_t REPEATS = 100000;

int main() {
    uint64_t sum_miss = 0;
    uint64_t sum_hit = 0;
    for (size_t i = 0; i < REPEATS; i++) {
        page_t* page_list = calloc(UINT8_MAX + 1, PAGE_SIZE);
        assert(page_list != NULL);
        flush_cache_line(page_list);
        uint64_t first_miss = time_read(page_list);
        uint64_t hit = time_read(page_list);
        if (hit <= first_miss) {
            sum_miss += first_miss;
            sum_hit += hit;
        }
    }
    printf("average miss = %" PRIu64 "\n", sum_miss / REPEATS);
    printf("average hit  = %" PRIu64 "\n", sum_hit / REPEATS);
}
