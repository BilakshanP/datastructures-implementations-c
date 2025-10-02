#include "hashset.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

/******************************************************************************
 *                                                                            *
 *                              Inner Functions                               *
 *                                                                            *
 ******************************************************************************/

/// Get next power of `2`, using compiler builtins (count leading zeroes).
inline static uint64_t __npo2(uint64_t n);
uint64_t __random_u64(void);
inline static uint64_t __best_capacity(uint64_t curr_count, double load_factor);
inline static uint64_t __hs_hash(const HSet* hs, const void* k);
inline static size_t __hs_index(uint64_t hash, size_t capacity);
// no null checks, get the node with this element
inline static HSNode* __hs_find(HSet* hs, const void* k);
// returns the node at which data is found, NULL if not
inline static HSNode** __hs_find_target_ptr(HSet* hs, const void* k, uint64_t hash, size_t index);
inline static bool __hs_contains(HSet* hs, const void* k, uint64_t hash, size_t index);
// New helper function for resizing and re-hashing
static bool __hs_resize(HSet* hs, size_t new_capacity);
inline static HSNode* __hs_new_node(void* k, uint64_t hash);
inline static void __hs_free_node(HSNode* node, void (*deallocator)(void* k));

/******************************************************************************
 *                                                                            *
 *                               Intialization                                *
 *                                                                            *
 ******************************************************************************/

HSet* hs_new(
    size_t element_size,
    int (*cmp)(const void* a, const void* b),
    uint64_t (*hasher)(const void* k, uint64_t seed_0, uint64_t seed_1),
    void (*copier)(void* dest, const void* src),
    void (*deallocator)(void* k)  //
) {
    return hs_new_with_capacity(element_size, cmp, hasher, copier, deallocator, 4);
}

HSet* hs_new_with_capacity(
    size_t element_size,
    int (*cmp)(const void* a, const void* b),
    uint64_t (*hasher)(const void* k, uint64_t seed_0, uint64_t seed_1),
    void (*copier)(void* dest, const void* src),
    void (*deallocator)(void* k),
    size_t capacity  //
) {
    if (element_size == 0 || !cmp || !deallocator || !copier || !hasher) return NULL;

    HSet* hs = (HSet*)malloc(sizeof(HSet));
    if (!hs) return NULL;

    capacity = capacity < 4 ? 4 : __npo2(capacity);

    HSNode** buckets = (HSNode**)calloc(capacity, sizeof(HSNode*));
    if (!buckets) {
        free(hs);
        return NULL;
    }

    hs->count = 0;
    hs->capacity = capacity;
    hs->element_size = element_size;

    hs->buckets = buckets;

    uint64_t seed_0 = __random_u64();
    uint64_t seed_1 = __random_u64();

    hs->seed_0 = seed_0;
    hs->seed_1 = seed_1;

    hs->load_factor = 0.75;
    hs->growth_factor = 2.0;

    hs->cmp = cmp;
    hs->hasher = hasher;
    hs->copier = copier;
    hs->deallocator = deallocator;

    hs->_mut_count = 0;
    hs->_collisions = 0;

    return hs;
}

HSet* hs_new_from_array(
    size_t element_size,
    int (*cmp)(const void* a, const void* b),
    uint64_t (*hasher)(const void* k, uint64_t seed_0, uint64_t seed_1),
    void (*copier)(void* dest, const void* src),
    void (*deallocator)(void* k),
    const void* arr,
    size_t length  //
) {
    HSet* hs = hs_new_with_capacity(element_size, cmp, hasher, copier, deallocator, length);
    if (!hs) return NULL;

    for (size_t i = 0; i < length; i++) {
        void* elem = (char*)arr + (element_size * i);

        if (!hs_insert(hs, elem)) {
            hs_free(hs);
            return NULL;
        }
    }

    return hs;
}

/******************************************************************************
 *                                                                            *
 *                             Clean Up & Freeing                             *
 *                                                                            *
 ******************************************************************************/

void hs_free(HSet* hs) {
    if (!hs) return;

    hs_clear(hs);
    free(hs->buckets);
    free(hs);
}
// inner
void hs_clear(HSet* hs) {
    if (!hs) return;

    hs->_mut_count++;

    for (size_t i = 0; i < hs->capacity; i++) {
        HSNode* curr = hs->buckets[i];

        while (curr) {
            HSNode* next = curr->next;

            hs->deallocator(curr->key);

            free(curr);

            curr = next;
        }
    }
}

/******************************************************************************
 *                                                                            *
 *                               Basic Getters                                *
 *                                                                            *
 ******************************************************************************/

size_t hs_count(const HSet* hs) {
    return hs->count;
}
size_t hs_is_empty(const HSet* hs) {
    return hs->count == 0;
}

/******************************************************************************
 *                                                                            *
 *                                  Copiers                                   *
 *                                                                            *
 ******************************************************************************/

HSet* hs_copy(const HSet* hs) {
    HSet* copy = hs_copy_metadata(hs);
    if (!copy) return NULL;

    for (size_t i = 0; i < hs->capacity; i++) {
        HSNode* curr = hs->buckets[i];

        while (curr) {
            if (!hs_insert(copy, curr->key)) {
                hs_free(copy);
                return NULL;
            }

            curr = curr->next;
        }
    }

    return copy;
}

HSet* hs_copy_with_capacity(const HSet* hs, size_t capacity) {
    HSet* copy = hs_copy_metadata_with_capacity(hs, capacity);
    if (!copy) return NULL;

    for (size_t i = 0; i < hs->capacity; i++) {
        HSNode* curr = hs->buckets[i];

        while (curr) {
            if (!hs_insert(copy, curr->key)) {
                hs_free(copy);
                return NULL;
            }

            curr = curr->next;
        }
    }

    return copy;
}

HSet* hs_copy_metadata(const HSet* hs) {
    if (!hs) return NULL;

    HSet* copy = hs_new(hs->element_size, hs->cmp, hs->hasher, hs->copier, hs->deallocator);
    if (!copy) return NULL;

    return copy;
}

HSet* hs_copy_metadata_with_capacity(const HSet* hs, size_t capacity) {
    if (!hs) return NULL;

    HSet* copy = hs_new_with_capacity(hs->element_size, hs->cmp, hs->hasher, hs->copier, hs->deallocator, capacity);
    if (!copy) return NULL;

    return copy;
}

/******************************************************************************
 *                                                                            *
 *                                  Printing                                  *
 *                                                                            *
 ******************************************************************************/

void hs_print(const HSet* hs) {
    hs_fprint(stdout, hs);
}
void hs_fprint(FILE* file, const HSet* hs) {
    if (!file) file = stdout;

    if (!hs) {
        fprintf(file, "HSet(NULL)");
        return;
    }

    bool first = true;

    fprintf(file, "{");

    for (size_t i = 0; i < hs->capacity; i++) {
        HSNode* curr = hs->buckets[i];

        while (curr) {
            if (!first) printf(", ");
            first = false;

            if (hs->printer) {
                hs->printer(file, curr->key);
            } else {
                fprintf(file, "<@%p#%lu>", curr->key, curr->hash);
            }

            curr = curr->next;
        }
    }

    fprintf(file, "}");
}

void hs_fprint_debug(FILE* file, const HSet* hs) {
    if (!file) file = stdout;

    if (!hs) {
        fprintf(file, "HSet(NULL)");
        return;
    }

    fprintf(file, "{");

    for (size_t i = 0; i < hs->capacity; i++) {
        printf("[");

        HSNode* curr = hs->buckets[i];
        bool first = true;

        while (curr) {
            if (!first) printf(", ");
            first = false;

            if (hs->printer) {
                hs->printer(file, curr->key);
                fprintf(file, "#%lu", curr->hash);
            } else {
                fprintf(file, "<@%p#%lu>", curr->key, curr->hash);
            }

            curr = curr->next;
        }

        printf("]");

        if (i + 1 < hs->capacity) printf(", ");
    }

    fprintf(file, "}");
}

void hs_fprint_metadata(FILE* file, const HSet* hs) {
    if (!file) file = stdout;

    if (!hs) {
        fprintf(file, "HSet(@NULL)");
        return;
    }

    double usage = (double)hs->count / (double)hs->capacity;

    fprintf(file,
            "HSet(@%p, %lu/%lu, %.2lf/%.2lf, seed: (%lx, %lx), mutations: %lu, collisions: %lu)",
            (void*)hs,
            hs->count,
            hs->capacity,
            usage,
            hs->load_factor,
            hs->seed_0,
            hs->seed_1,
            hs->_mut_count,
            hs->_collisions);
}

/******************************************************************************
 *                                                                            *
 *                                   Resize                                   *
 *                                                                            *
 ******************************************************************************/

bool hs_resize(HSet* hs, size_t new_capacity) {
    if (!hs) return false;

    hs->_mut_count++;

    if (hs->capacity > new_capacity) return true;

    new_capacity = __npo2(new_capacity);

    return __hs_resize(hs, new_capacity);
}

/******************************************************************************
 *                                                                            *
 *                      Insertion, Deletion & Searching                       *
 *                                                                            *
 ******************************************************************************/

bool hs_insert(HSet* hs, const void* k) {
    if (!hs || !k) return false;

    hs->_mut_count++;

    uint64_t hash = __hs_hash(hs, k);
    size_t index = __hs_index(hash, hs->capacity);

    if (__hs_contains(hs, k, hash, index)) return false;

    void* k_copy = malloc(hs->element_size);
    if (!k_copy) return false;

    hs->copier(k_copy, k);

    HSNode* node = __hs_new_node(k_copy, hash);
    if (!node) {
        free(k_copy);
        return false;
    }

    if (hs->buckets[index]) hs->_collisions++;

    node->next = hs->buckets[index];
    hs->buckets[index] = node;

    hs->count++;

    if ((double)hs->count / (double)hs->capacity > hs->load_factor) __hs_resize(hs, hs->capacity * 2);

    return true;
}

bool hs_remove(HSet* hs, const void* k) {
    if (!hs || !k) return false;

    hs->_mut_count++;

    uint64_t hash = __hs_hash(hs, k);
    size_t index = __hs_index(hash, hs->capacity);

    HSNode** target_ptr_addr = __hs_find_target_ptr(hs, k, hash, index);

    if (!target_ptr_addr) return false;

    // *target_ptr_addr is the pointer (HSNode*) that points to the node we want to remove
    HSNode* node_to_remove = *target_ptr_addr;

    hs->deallocator(node_to_remove->key);

    // Unlink the node: The pointer that currently points to node_to_remove
    // is now made to point to the node_to_remove's next node.
    *target_ptr_addr = node_to_remove->next;

    free(node_to_remove);

    hs->count--;

    return true;
}

void hs_retain(HSet* hs, bool (*predicate)(void* k)) {
    if (!hs || !predicate) return;

    hs->_mut_count++;

    for (size_t i = 0; i < hs->capacity; i++) {
        // We need a pointer to the pointer (HSNode**) to manage unlinking,
        // starting at the bucket head.
        HSNode** curr_ptr_addr = &hs->buckets[i];

        while (*curr_ptr_addr) {
            HSNode* curr = *curr_ptr_addr;

            if (predicate(curr->key)) {
                // NEXT: The predicate returned truee.
                curr_ptr_addr = &curr->next;
            } else {
                // REMOVE: The predicate returned false.
                hs->deallocator(curr->key);

                *curr_ptr_addr = curr->next;

                HSNode* node_to_free = curr;
                free(node_to_free);

                hs->count--;
            }
        }
    }
}

bool hs_contains(const HSet* hs, const void* k) {
    if (!hs || !k) return false;

    uint64_t hash = __hs_hash(hs, k);
    size_t index = __hs_index(hash, hs->capacity);

    return __hs_find_target_ptr((HSet*)hs, k, hash, index) != NULL;
}

/******************************************************************************
 *                                                                            *
 *                              Advanced Getters                              *
 *                                                                            *
 ******************************************************************************/

void* hs_extract(const HSet* hs) {
    if (!hs) return NULL;

    void* arr = malloc(hs->element_size * hs->count);

    size_t n = 0;

    for (size_t i = 0; i < hs->capacity; i++) {
        HSNode* curr = hs->buckets[i];

        while (curr) {
            void* dest = (char*)arr + (hs->element_size * n);
            hs->copier(dest, curr->key);

            n++;
            curr = curr->next;
        }
    }

    return arr;
}

/******************************************************************************
 *                                                                            *
 *                                Comparators                                 *
 *                                                                            *
 ******************************************************************************/

bool hs_are_eq(const HSet* a, const HSet* b) {
    if (a == b) return true;
    if (!a || !b) return false;

    // Must have the same number of elements for equality
    if (a->count != b->count) return false;

    // They must use the same element size, though this doesn't guarantee data compatibility
    if (a->element_size != b->element_size) return false;

    // Check if every element in A is contained in B (A is a subset of B).
    // If counts are equal AND A is a subset of B, then A = B.
    for (size_t i = 0; i < a->capacity; i++) {
        HSNode* curr = a->buckets[i];

        while (curr) {
            if (!hs_contains(b, curr->key)) return false;

            curr = curr->next;
        }
    }

    return true;
}

bool hs_are_disjoint(const HSet* a, const HSet* b) {
    if (a == b) return (a && a->count > 0) ? false : true;  // Disjoint only if both are NULL or empty.
    if (!a || !b) return true;                              // Treating a NULL set as empty and disjoint from any other set.

    if (a->element_size != b->element_size) return true;  // Cannot share elements, so they are disjoint.

    // Optimize: Iterate over the smaller set for faster checks
    const HSet* smaller = (a->count <= b->count) ? a : b;
    const HSet* larger = (a->count <= b->count) ? b : a;

    for (size_t i = 0; i < smaller->capacity; i++) {
        HSNode* curr = smaller->buckets[i];

        while (curr) {
            // If any element in the smaller set is in the larger set, they are NOT disjoint.
            if (hs_contains(larger, curr->key)) return false;

            curr = curr->next;
        }
    }

    return true;
}

bool hs_is_subset(const HSet* a, const HSet* b) {
    if (a == b) return true;
    if (!a || !b) return hs_is_empty(a);  // If A is NULL/empty, it's a subset of B. If B is NULL, only NULL A works.

    if (a->count > b->count) return false;  // A cannot be a subset of B if it's larger

    // Check if every element in A is contained in B.
    for (size_t i = 0; i < a->capacity; i++) {
        HSNode* curr = a->buckets[i];

        while (curr) {
            if (!hs_contains(b, curr->key)) return false;

            curr = curr->next;
        }
    }

    return true;
}

bool hs_is_supset(const HSet* a, const HSet* b) {
    // A is a superset of B (A ⊇ B) if B is a subset of A (B ⊆ A).
    return hs_is_subset(b, a);
}

/******************************************************************************
 *                                                                            *
 *                            Algebric Operations                             *
 *                                                                            *
 ******************************************************************************/

HSet* hs_union(const HSet* a, const HSet* b) {
    if (!a) return hs_copy(b);
    if (!b) return hs_copy(a);

    // Optimize: Start by copying the larger set to minimize insertions.
    const HSet* first = (a->count >= b->count) ? a : b;
    const HSet* second = (a->count >= b->count) ? b : a;

    size_t capacity = __best_capacity(a->count + b->count, a->load_factor);
    HSet* union_ab = hs_copy_with_capacity(first, capacity);
    if (!union_ab) return NULL;

    // Insert all elements from the smaller set into the copy of the larger set.
    // hs_insert handles duplicates by returning false, which is fine.
    for (size_t i = 0; i < second->capacity; i++) {
        HSNode* curr = second->buckets[i];

        while (curr) {
            // We insert the key pointer, relying on hs_insert/hs_new_node to handle deep copy if needed.
            hs_insert(union_ab, curr->key);

            curr = curr->next;
        }
    }

    return union_ab;
}

// NOTE: I am assuming a helper like hs_new_with_metadata exists to handle the setup.
HSet* hs_intersection(const HSet* a, const HSet* b) {
    if (!a) return hs_copy_metadata(b);
    if (!b) return hs_copy_metadata(a);

    // Optimize: Iterate over the smaller set for efficiency.
    const HSet* iterate_set = (a->count <= b->count) ? a : b;
    const HSet* check_set = (a->count <= b->count) ? b : a;

    // Create a new empty hash set with metadata copied from one of the operands (e.g., 'a')
    // This is a necessary step that assumes the creation of a new empty set helper.
    HSet* intersection_ab = hs_copy_metadata_with_capacity(a, check_set->capacity);
    if (!intersection_ab) return NULL;
    // NOTE: If you need other metadata (seeds, factors) use hs_copy_metadata here.

    for (size_t i = 0; i < iterate_set->capacity; i++) {
        HSNode* curr = iterate_set->buckets[i];

        while (curr) {
            // Check if the element exists in the other set
            if (hs_contains(check_set, curr->key)) {
                // If it exists in both, insert into the result set
                // Insert handles key allocation/duplication logic
                hs_insert(intersection_ab, curr->key);
            }

            curr = curr->next;
        }
    }

    return intersection_ab;
}

HSet* hs_difference(const HSet* a, const HSet* b) {
    if (!a) return hs_copy_metadata(b);  // A \ B = empty set (NULL) if A is NULL.
    if (!b) return hs_copy(a);           // A \ B = A if B is NULL.

    // Create a new empty hash set with metadata copied from 'a'
    HSet* difference_ab = hs_copy_metadata_with_capacity(a, a->capacity);
    if (!difference_ab) return NULL;
    // NOTE: Copy other metadata (seeds, factors) if needed.

    // Iterate over A and include elements not found in B.
    for (size_t i = 0; i < a->capacity; i++) {
        HSNode* curr = a->buckets[i];

        while (curr) {
            if (!hs_contains(b, curr->key)) {
                hs_insert(difference_ab, curr->key);
            }

            curr = curr->next;
        }
    }

    return difference_ab;
}

HSet* hs_sym_difference(const HSet* a, const HSet* b) {
    if (!a) return hs_copy(b);
    if (!b) return hs_copy(a);

    // Option 1: Use the difference functions
    // HSet* diff_a_b = hs_difference(a, b);
    // HSet* diff_b_a = hs_difference(b, a);
    // HSet* result = hs_union(diff_a_b, diff_b_a);
    // hs_free(diff_a_b); // Free intermediate sets
    // hs_free(diff_b_a);
    // return result;

    // Option 2: Build the set by iterating both and checking containment.

    // Create new empty hash set with metadata copied from 'a'
    size_t capacity = __best_capacity(a->count + b->count, a->load_factor);
    HSet* sym_difference_ab = hs_copy_metadata_with_capacity(a, capacity);
    if (!sym_difference_ab) return NULL;

    // Pass 1: Add elements from A not in B (A \ B)
    for (size_t i = 0; i < a->capacity; i++) {
        HSNode* curr = a->buckets[i];
        while (curr) {
            if (!hs_contains(b, curr->key)) {
                hs_insert(sym_difference_ab, curr->key);
            }

            curr = curr->next;
        }
    }

    // Pass 2: Add elements from B not in A (B \ A)
    for (size_t i = 0; i < b->capacity; i++) {
        HSNode* curr = b->buckets[i];
        while (curr) {
            // Note: We check if the element is in A. Since the result set is new,
            // inserting duplicates is handled by hs_insert, but they shouldn't occur
            // if we are correctly checking B \ A.
            if (!hs_contains(a, curr->key)) {
                hs_insert(sym_difference_ab, curr->key);
            }

            curr = curr->next;
        }
    }

    return sym_difference_ab;
}

HSet* hs_filter(const HSet* hs, bool (*predicate)(void* k)) {
    if (!hs || !predicate) return NULL;

    // Create a new empty hash set with metadata copied from 'hs'
    HSet* filtered_hs = hs_copy_metadata_with_capacity(hs, hs->capacity);
    if (!filtered_hs) return NULL;
    // NOTE: Copy other metadata (seeds, factors) if needed.

    for (size_t i = 0; i < hs->capacity; i++) {
        HSNode* curr = hs->buckets[i];

        while (curr) {
            // If the predicate is true, insert the element into the new set.
            if (predicate(curr->key)) {
                hs_insert(filtered_hs, curr->key);
            }

            curr = curr->next;
        }
    }

    return filtered_hs;
}

/******************************************************************************
 *                                                                            *
 *                                  Iterator                                  *
 *                                                                            *
 ******************************************************************************/

HSIterator* hs_iterator(HSet* hs) {
    HSIterator* it = malloc(sizeof(HSIterator));
    if (!it) return NULL;

    it->hs = hs;
    it->index = 0;
    it->node = NULL;
    it->mutations = hs->_mut_count;

    return it;
}

bool hs_iter_next(HSIterator* it) {
    if (!it || !it->hs) return false;
    if (it->mutations != it->hs->_mut_count) return false;

    // if currently inside a bucket, move to the next node
    if (it->node) {
        it->node = it->node->next;
        if (it->node) return true;
    }

    // otherwise, move to the next non-empty node
    while (it->index < it->hs->capacity) {
        HSNode* node = it->hs->buckets[it->index++];
        if (node) {
            it->node = node;
            return true;
        }
    }

    // no more elements
    it->node = NULL;

    return false;
}

void* hs_iter_get(HSIterator* it) {
    if (!it || !it->node) return NULL;
    if (it->mutations != it->hs->_mut_count) return NULL;

    return it->node->key;
}

/******************************************************************************
 *                                                                            *
 *                       Inner Functions Implementation                       *
 *                                                                            *
 ******************************************************************************/

inline static uint64_t __npo2(uint64_t n) {
    return n == 1 ? 1 : 1 << (64 - __builtin_clzl(n - 1));
}

uint64_t __random_u64(void) {
    uint64_t val;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("open /dev/urandom");
        exit(EXIT_FAILURE);
    }

    ssize_t n = read(fd, &val, sizeof(val));

    if (n != sizeof(val)) {
        perror("read /dev/urandom");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);

    return val;
}

inline static uint64_t __best_capacity(uint64_t curr_count, double load_factor) {
    double capacity = (double)curr_count / load_factor;
    size_t final_capacity = (size_t)capacity;
    // final_capacity = __npo2(final_capacity);
    return final_capacity;
}

inline static uint64_t __hs_hash(const HSet* hs, const void* k) {
    return hs->hasher(k, hs->seed_0, hs->seed_1);
}

inline static size_t __hs_index(uint64_t hash, size_t capacity) {
    return hash & (capacity - 1);
}

inline static HSNode* __hs_find(HSet* hs, const void* k) {
    uint64_t hash = __hs_hash(hs, k);
    size_t index = __hs_index(hash, hs->capacity);

    HSNode* curr = hs->buckets[index];

    while (curr) {
        if (curr->hash == hash && hs->cmp(curr->key, k) == 0) {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

inline static HSNode** __hs_find_target_ptr(HSet* hs, const void* k, uint64_t hash, size_t index) {
    HSNode** curr_ptr = &hs->buckets[index];

    while (*curr_ptr) {
        HSNode* curr = *curr_ptr;

        if (curr->hash == hash && hs->cmp(curr->key, k) == 0) {
            return curr_ptr;  // Found the address of the pointer pointing to the target node
        }

        curr_ptr = &curr->next;
    }

    return NULL;  // Element not found
}

inline static bool __hs_contains(HSet* hs, const void* k, uint64_t hash, size_t index) {
    HSNode** node = __hs_find_target_ptr(hs, k, hash, index);
    if (node) return true;

    return false;
}

static bool __hs_resize(HSet* hs, size_t new_capacity) {
    // 1. Validate the new capacity
    if (new_capacity <= hs->capacity) {
        return false;  // Can only grow
    }

    // 2. Allocate the new bucket array (initialized to NULL)
    HSNode** new_buckets = (HSNode**)calloc(new_capacity, sizeof(HSNode*));
    if (!new_buckets) {
        return false;
    }

    // 3. Re-hash all existing nodes into the new buckets
    for (size_t i = 0; i < hs->capacity; i++) {
        HSNode* curr = hs->buckets[i];

        // Traverse the linked list in the old bucket
        while (curr) {
            HSNode* node_to_move = curr;
            curr = curr->next;  // Advance curr BEFORE unlinking node_to_move

            // Calculate the new index
            size_t new_index = __hs_index(node_to_move->hash, new_capacity);

            // Prepend the node to the new bucket (O(1) insertion)
            node_to_move->next = new_buckets[new_index];
            new_buckets[new_index] = node_to_move;
        }
    }

    // 4. Clean up and update the HSet structure
    free(hs->buckets);  // Free the old bucket array

    hs->buckets = new_buckets;
    hs->capacity = new_capacity;

    return true;
}

inline static HSNode* __hs_new_node(void* k, uint64_t hash) {
    HSNode* node = (HSNode*)malloc(sizeof(HSNode));
    if (!node) return NULL;

    node->key = k;
    node->hash = hash;
    node->next = NULL;

    return node;
}

inline static void __hs_free_node(HSNode* node, void (*deallocator)(void* k)) {
    deallocator(node->key);
    free(node);
}