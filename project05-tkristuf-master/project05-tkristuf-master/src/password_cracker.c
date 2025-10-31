#define _GNU_SOURCE
#include <assert.h>
#include <crypt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary_words.h"
#include "thread_pool.h"

const char HASH_START[] = "$6$";
const size_t SALT_LENGTH = 20;
const size_t HASH_LENGTH = 106;
const size_t NUM_THREADS = 16;

static size_t hash_count = 0;
static char **hashes = NULL;

static inline bool hashes_match(const char *password, const char *hash) {
    char salt[SALT_LENGTH + 1];
    memcpy(salt, hash, sizeof(char[SALT_LENGTH]));
    salt[SALT_LENGTH] = '\0';
    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *hashed = crypt_r(password, salt, &data);
    char *hashed_hash = &hashed[SALT_LENGTH];
    const char *hash_hash = &hash[SALT_LENGTH];
    return memcmp(hashed_hash, hash_hash, sizeof(char[HASH_LENGTH - SALT_LENGTH])) == 0;
}

void brute_force_method(void *dictionary_word) {
    char *original_word = (char *) dictionary_word;
    size_t original_length = strlen(original_word);
    char augmented_word[original_length + 2];

    for (size_t i = 0; i <= original_length; i++) {
        for (size_t d = 0; d < 10; d++) {
            strncpy(augmented_word, original_word, i);
            augmented_word[i] = '0' + d;
            strcpy(augmented_word + i + 1, original_word + i);

            for (size_t j = 0; j < hash_count; j++) {
                if (hashes_match(augmented_word, hashes[j])) {
                    printf("%s\n", augmented_word);
                }
            }
        }
    }
}

int main(void) {
    // Read in the hashes from the standard input
    char *line = NULL;
    size_t line_capacity = 0;
    // Stop when the end of the input or an empty line is reached
    while (getline(&line, &line_capacity, stdin) > 0 && line[0] != '\n') {
        // Check that the line looks like a hash
        size_t line_length = strlen(line);
        assert(line_length == HASH_LENGTH ||
               (line_length == HASH_LENGTH + 1 && line[HASH_LENGTH] == '\n'));
        assert(memcmp(line, HASH_START, sizeof(HASH_START) - sizeof(char)) == 0);

        // Extend the hashes array and add the hash to it
        hashes = realloc(hashes, sizeof(char * [hash_count + 1]));
        assert(hashes != NULL);
        char *hash = malloc(sizeof(char[HASH_LENGTH + 1]));
        assert(hash != NULL);
        memcpy(hash, line, sizeof(char[HASH_LENGTH]));
        hash[HASH_LENGTH] = '\0';
        hashes[hash_count++] = hash;
    }
    free(line);

    thread_pool_t *thread_pool = thread_pool_init(NUM_THREADS);
    for (size_t i = 0; i < NUM_DICTIONARY_WORDS; i++) {
        thread_pool_add_work(thread_pool, brute_force_method, (void *) DICTIONARY[i]);
    }
    thread_pool_finish(thread_pool);
}
