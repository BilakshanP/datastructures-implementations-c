#define DYNAMICARRAY_IMPL

#include "darray.h"

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
 *                                                                            *
 *                              Inner Functions                               *
 *                                                                            *
 ******************************************************************************/

/// @brief Default element copier function.
/// @details This function is intentionally designed to trigger a program halt
/// using `raise(SIGTRAP)` if the user fails to set a custom copier. For complex
/// data types, using `memcpy` (the C default) leads to shallow copies and is
/// usually incorrect.
/// @note **CONTRACT VIOLATION EXIT BEHAVIOR:** If executed, prints "Set copier"
/// to stderr and terminates the program.
/// @param dest Pointer to the destination memory.
/// @param src Pointer to the source memory.
inline static void __da_default_copier(void* dest, const void* src);

/// @brief Default element deallocator function.
/// @details Performs no operation. This is suitable for primitive data types (like `int`, `double`, or simple structs) that do not own any heap-allocated resources.
/// @param k Pointer to the element; currently unused.
inline static void __da_default_deallocator(void* k);

/// @brief Default element printer function.
/// @details Prints the memory address of the element in the format `<@ADDRESS>`.
/// @param k Pointer to the element to be printed.
inline static void __da_default_printer(const void* k);

/// @brief Calculates the memory address of the element at a given index.
/// @details Internal function for raw pointer arithmetic: `(char*)da->arr + (da->element_size * idx)`.
/// @param da Pointer to the dynamic array.
/// @param idx The index of the element.
/// @return A constant pointer to the memory location of the element.
inline static const void* __da_index_raw(const DArray* da, size_t idx);

/// @brief Copies an element into the array at the specified index using the configured copier.
/// @details A wrapper around `da->copier` that calculates the destination address. This function does **not** check bounds or update the array length.
/// @param da Pointer to the dynamic array.
/// @param idx The index where the element should be placed.
/// @param e Pointer to the source element data.
inline static void __da_set_raw(DArray* da, size_t idx, const void* e);

/// @brief Checks if reallocation is necessary and performs an upsizing (growth).
/// @details If `da->length == da->capacity`, it calculates a new capacity using `da->growth_factor` (minimum capacity of 1 if current is 0) and calls `da_reserve`.
/// @param da Pointer to the dynamic array.
/// @return `true` if the capacity is sufficient or resize succeeded, `false` on allocation failure.
inline static bool __da_upsize(DArray* da);

/// @brief Checks if reallocation is necessary and performs a downsizing (shrink).
/// @details If `da->length` is less than `da->capacity * da->shrink_factor`, it calls `da_shrink` to resize the array to fit the current length.
/// @param da Pointer to the dynamic array.
inline static void __da_downsize(DArray* da);

/// @brief Shifts a block of elements within the array using `memmove`.
/// @details Used for insertion (`dest_idx > src_idx`) or removal (`dest_idx < src_idx`).
/// @param da Pointer to the dynamic array.
/// @param dest_idx The starting index of the destination.
/// @param src_idx The starting index of the source. The number of elements shifted is from `src_idx` to `da->length - 1`.
inline static void __da_shift(DArray* da, size_t dest_idx, size_t src_idx);

/// @brief Swaps two elements in a raw array using a temporary buffer.
/// @param arr Pointer to the raw array memory.
/// @param t Pointer to a temporary buffer of size `element_size`.
/// @param element_size Size of a single element.
/// @param i Index of the first element.
/// @param j Index of the second element.
inline static void __da_swap(void* arr, char* t, size_t element_size, size_t i, size_t j);

/// @brief Reverses the order of elements within a specified range in a raw array.
/// @details Implements a standard in-place reversal algorithm using a temporary buffer for swapping. The `end` index is exclusive in the parameter list but is adjusted internally to be inclusive for the last element.
/// @param arr Pointer to the raw array memory.
/// @param element_size Size of a single element.
/// @param start The starting index of the range (inclusive).
/// @param end The ending index of the range (exclusive).
static void __da_reverse(void* arr, size_t element_size, size_t start, size_t end);

/******************************************************************************
 *                                                                            *
 *                               Intialization                                *
 *                                                                            *
 ******************************************************************************/

DArray* da_new(const size_t element_size) {
    return da_new_with_capacity(element_size, 4);
}

DArray* da_new_with_capacity(const size_t element_size, const size_t capacity) {
    DArray* da = malloc(sizeof(DArray));
    if (!da) return NULL;

    da->arr = malloc(capacity * element_size);
    if (!da->arr) {
        free(da);
        return NULL;
    }

    da->length = 0;
    da->capacity = capacity;
    da->element_size = element_size;

    da->growth_factor = 2.0;
    da->shrink_factor = 0.2;

    da->copier = __da_default_copier;
    da->deallocator = __da_default_deallocator;
    da->printer = __da_default_printer;

    return da;
}

DArray* da_new_from_array(const size_t element_size, const size_t length, const void* arr, void (*copier)(void* dest, const void* src)) {
    DArray* da = da_new_with_capacity(element_size, length);
    if (!da) return NULL;

    da->copier = copier;

    for (size_t i = 0; i < length; i++) {
        void* elem = (char*)arr + (element_size * i);
        __da_set_raw(da, i, elem);
    }

    da->length = length;

    return da;
}

/******************************************************************************
 *                                                                            *
 *                             Clean Up & Freeing                             *
 *                                                                            *
 ******************************************************************************/

DArray* da_copy(const DArray* da) {
    if (!da) return NULL;

    DArray* copied = da_new_with_capacity(da->element_size, da->length);
    if (!copied) return NULL;

    for (size_t i = 0; i < da->length; i++) {
        const void* src = __da_index_raw(da, i);
        void* dest = da_index(copied, i);

        da->copier(dest, src);
    }

    copied->length = da->length;
    copied->capacity = da->length;

    copied->copier = da->copier;
    copied->deallocator = da->deallocator;
    copied->printer = da->printer;

    return copied;
}

void da_free(DArray* da) {
    if (!da_clear(da)) return;

    free(da->arr);
    free(da);
}

bool da_clear(DArray* da) {
    if (!da) return false;

    if (da->deallocator) {
        for (size_t i = 0; i < da->length; i++) {
            void* elem = da_index(da, i);
            da->deallocator(elem);
        }
    }

    da->length = 0;

    return true;
}

/******************************************************************************
 *                                                                            *
 *                               Basic Getters                                *
 *                                                                            *
 ******************************************************************************/

inline size_t da_length(const DArray* da) {
    return da->length;
}

inline size_t da_capacity(const DArray* da) {
    return da->capacity;
}

inline bool da_is_empty(const DArray* da) {
    if (!da) return true;

    return da->length == 0;
}

inline void* da_index(DArray* da, size_t idx) {
    return (void*)__da_index_raw(da, idx);
}

/******************************************************************************
 *                                                                            *
 *                                  Printing                                  *
 *                                                                            *
 ******************************************************************************/

void da_print(const DArray* da) {
    da_fprint(da, stdout);
}

void da_fprint(const DArray* da, FILE* file) {
    if (!da) {
        printf("[NULLPTR]");
        return;
    }

    if (!file) file = stdout;

    fprintf(file, "[");

    for (size_t i = 0; i < da->length; i++) {
        const void* elem = __da_index_raw(da, i);
        da->printer(elem);
        if (i < da->length - 1) {
            fprintf(file, ", ");
        }
    }

    fprintf(file, "]");
}

/******************************************************************************
 *                                                                            *
 *                              Advanced Getters                              *
 *                                                                            *
 ******************************************************************************/

void* da_raw(DArray* da) {
    if (!da) return NULL;

    return da->arr;
}

void* da_get_raw(DArray* da) {
    void* arr = da->arr;

    free(da);

    return arr;
}

void* da_get_arr(const DArray* da) {
    if (!da) return NULL;

    void* arr = malloc(da->element_size * da->length);
    if (!arr) return NULL;

    for (size_t i = 0; i < da->length; i++) {
        const void* src = __da_index_raw(da, i);
        void* dest = (char*)arr + da->element_size * i;
        da->copier(dest, src);
    }

    return arr;
}

DArray* da_get_subarr(const DArray* da, size_t start, size_t end) {
    if (!da || start > end || end > da->length) return NULL;

    const size_t sub_len = end - start;
    DArray* sub = da_new_with_capacity(da->element_size, sub_len);
    if (!sub) return NULL;

    for (size_t i = 0; i < sub_len; i++) {
        const void* elem = __da_index_raw(da, start + i);
        void* dest = da_index(sub, i);
        da->copier(dest, elem);
    }

    sub->length = sub_len;

    return sub;
}

void* da_get(DArray* da, size_t idx) {
    if (!da || idx >= da->length) return NULL;

    return da_index(da, idx);
}

void* da_get_first(DArray* da) {
    return da_get(da, 0);
}

void* da_get_last(DArray* da) {
    if (!da || da_is_empty(da)) return NULL;

    return da_get(da, da->length - 1);
}

/******************************************************************************
 *                                                                            *
 *                                 Searching                                  *
 *                                                                            *
 ******************************************************************************/

size_t da_find(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b)) {
    if (!da || !target || !cmp) return (size_t)-1;

    for (size_t i = 0; i < da->length; i++) {
        const void* elem = __da_index_raw(da, i);

        if (cmp(elem, target) == 0) {
            return i;
        }
    }

    return (size_t)-1;
}

size_t da_binary_search(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b)) {
    if (!da || !target || !cmp) return (size_t)-1;

    size_t low = 0;
    size_t high = da->length - 1;

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        const void* elem = __da_index_raw(da, mid);
        int cmp_res = cmp(elem, target);

        if (cmp_res < 0) {
            low = mid + 1;
        } else if (cmp_res > 0) {
            high = mid - 1;
        } else {
            return mid;
        }
    }

    return (size_t)-1;
}

bool da_contains(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b)) {
    return da_find(da, target, cmp) != (size_t)-1;
}

bool da_contains_bsearch(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b)) {
    if (!da | !target) return false;

    return bsearch(target, da->arr, da->length, da->element_size, cmp) != NULL;
}

/******************************************************************************
 *                                                                            *
 *                                  Setters                                   *
 *                                                                            *
 ******************************************************************************/

bool da_set(DArray* da, size_t idx, const void* e) {
    if (!da || idx >= da->capacity || !e) return false;

    __da_set_raw(da, idx, e);

    if (idx >= da->length) da->length = idx + 1;

    return true;
}

bool da_swap(DArray* da, size_t i, size_t j) {
    if (!da || i >= da->length || j >= da->length) return false;

    if (i == j) return true;

    char* buffer = (char*)malloc(da->element_size);
    if (!buffer) return false;

    __da_swap(da->arr, buffer, da->element_size, i, j);

    free(buffer);

    return true;
}

/******************************************************************************
 *                                                                            *
 *                            Insertion & Deletion                            *
 *                                                                            *
 ******************************************************************************/

bool da_push(DArray* da, const void* e) {
    if (!da || !e) return false;
    if (!__da_upsize(da)) return false;

    __da_set_raw(da, da->length, e);
    da->length++;

    return true;
}

bool da_push_front(DArray* da, const void* e) {
    if (!da || !e) return false;
    if (!__da_upsize(da)) return false;

    __da_shift(da, 1, 0);
    __da_set_raw(da, 0, e);

    da->length++;

    return true;
}

void* da_pop(DArray* da) {
    if (!da || da_is_empty(da)) return NULL;

    size_t idx = da->length - 1;
    void* src = da_index(da, idx);

    void* elem = malloc(da->element_size);
    if (!elem) return NULL;

    da->copier(elem, src);

    da->deallocator(src);

    da->length--;

    __da_downsize(da);

    return elem;
}

void* da_pop_front(DArray* da) {
    if (!da || da_is_empty(da)) return NULL;

    void* src = da_index(da, 0);

    void* elem = malloc(da->element_size);
    if (!elem) return NULL;

    da->copier(elem, src);

    da->deallocator(src);

    __da_shift(da, 0, 1);

    da->length--;

    __da_downsize(da);

    return elem;
}

size_t da_remove(DArray* da, void* target, int (*cmp)(const void* a, const void* b)) {
    if (!da || !target || !cmp) return (size_t)-1;

    for (size_t i = 0; i < da->length; i++) {
        void* curr = da_index(da, i);
        if (cmp(curr, target) == 0) {
            da->deallocator(curr);

            __da_shift(da, i, i + 1);

            da->length--;

            __da_downsize(da);

            return i;
        }
    }

    return (size_t)-1;
}

bool da_insert_at(DArray* da, size_t idx, const void* e) {
    if (!da || !e || idx > da->length) return false;

    if (idx == 0) return da_push_front(da, e);
    if (idx == da->length) return da_push(da, e);

    if (!__da_upsize(da)) return false;

    __da_shift(da, idx + 1, idx);
    __da_set_raw(da, idx, e);

    da->length++;

    return true;
}

bool da_remove_at(DArray* da, size_t idx) {
    if (!da || idx >= da->length) return false;

    if (idx == 0) return da_pop_front(da);

    if (idx == da->length - 1) return da_pop(da);

    void* elem = da_index(da, idx);
    da->deallocator(elem);

    __da_shift(da, idx, idx + 1);

    da->length--;

    __da_downsize(da);

    return true;
}

/******************************************************************************
 *                                                                            *
 *                                  Resizing                                  *
 *                                                                            *
 ******************************************************************************/

bool da_truncate(DArray* da, size_t new_length) {
    if (!da) return false;

    if (new_length >= da->length) {
        return true;
    }

    if (da->deallocator) {
        for (size_t i = new_length; i < da->length; i++) {
            void* elem = (char*)da->arr + (da->element_size * i);
            da->deallocator(elem);
        }
    }

    da->length = new_length;

    return true;
}

bool da_resize(DArray* da, size_t capacity) {
    if (!da) return false;

    if (da->capacity == capacity) return true;

    if (da->length > capacity) {
        if (!da_truncate(da, capacity)) return false;
    }

    void* new_arr = realloc(da->arr, da->element_size * capacity);

    if (!new_arr && capacity > 0) {
        return false;
    }

    da->arr = new_arr;
    da->capacity = capacity;

    return true;
}

bool da_reserve(DArray* da, size_t capacity) {
    if (!da) return false;

    if (da->capacity >= capacity) return true;

    return da_resize(da, capacity);
}

bool da_shrink(DArray* da) {
    if (!da) return false;

    return da_resize(da, da->length);
}

/******************************************************************************
 *                                                                            *
 *                               Concatanation                                *
 *                                                                            *
 ******************************************************************************/

DArray* da_concat(const DArray* a, const DArray* b) {
    if (!a) return da_copy(b);
    if (!b) return da_copy(a);

    if (a->element_size != b->element_size) return NULL;

    size_t length = a->length + b->length;

    DArray* concatanated = da_new_with_capacity(a->element_size, length);
    if (!concatanated) return NULL;

    concatanated->copier = a->copier;
    concatanated->deallocator = a->deallocator;
    concatanated->printer = a->printer;

    size_t i = 0;

    while (i < a->length) {
        const void* src = __da_index_raw(a, i);
        __da_set_raw(concatanated, i, src);

        i++;
    }

    while (i < length) {
        const void* src = __da_index_raw(b, i - a->length);
        __da_set_raw(concatanated, i, src);

        i++;
    }

    concatanated->length = length;

    return concatanated;
}

DArray* da_merge_sorted(const DArray* a, const DArray* b, int (*cmp)(const void* a, const void* b)) {
    if (!a) return da_copy(b);
    if (!b) return da_copy(a);
    if (!cmp) return NULL;
    if (a->element_size != b->element_size) return NULL;

    size_t length = a->length + b->length;
    DArray* merged = da_new_with_capacity(a->element_size, length);
    if (!merged) return NULL;

    merged->copier = a->copier;
    merged->deallocator = a->deallocator;
    merged->printer = a->printer;

    size_t i = 0, ai = 0, bi = 0;

    while (ai < a->length && bi < b->length) {
        const void* src;
        const void* src_a = __da_index_raw(a, ai);
        const void* src_b = __da_index_raw(b, bi);

        if (cmp(src_a, src_b) <= 0) {
            src = src_a;
            ai++;
        } else {
            src = src_b;
            bi++;
        }

        void* dest = da_index(merged, i++);
        merged->copier(dest, src);
    }

    while (ai < a->length) {
        const void* src_a = __da_index_raw(a, ai++);
        void* dest = da_index(merged, i++);
        merged->copier(dest, src_a);
    }

    while (bi < b->length) {
        const void* src_b = __da_index_raw(b, bi++);
        void* dest = da_index(merged, i++);
        merged->copier(dest, src_b);
    }

    merged->length = i;

    return merged;
}

/******************************************************************************
 *                                                                            *
 *                        Sorting & Order Manipulation                        *
 *                                                                            *
 ******************************************************************************/

void da_sort(DArray* da, int (*cmp)(const void* a, const void* b)) {
    if (!da || !cmp) return;

    qsort(da->arr, da->length, da->element_size, cmp);
}

void da_reverse(DArray* da) {
    if (!da) return;

    size_t idx = da->length - 1;  // last index
    __da_reverse(da->arr, da->element_size, 0, idx);
}

void da_rotate_left(DArray* da, size_t k) {
    if (!da) return;

    size_t n = da->length;
    if (n == 0 || k == 0) return;

    k %= n;
    if (k == 0) return;

    // Standard Three-Reversal Logic: (A B) -> (A^R B^R) -> (B A)

    __da_reverse(da->arr, da->element_size, 0, k - 1);
    __da_reverse(da->arr, da->element_size, k, n - 1);
    __da_reverse(da->arr, da->element_size, 0, n - 1);
}

void da_rotate_right(DArray* da, size_t k) {
    if (!da) return;

    size_t n = da->length;
    if (n == 0) return;

    k %= n;
    if (k == 0) return;

    da_rotate_left(da, n - k);
}

/******************************************************************************
 *                                                                            *
 *                             Functional Methods                             *
 *                                                                            *
 ******************************************************************************/

DArray* da_map(const DArray* da, void (*map_fn)(void* dest, const void* src), const size_t out_element_size) {
    if (!da || !map_fn || out_element_size == 0) return NULL;

    const size_t n = da->length;

    DArray* mapped = da_new_with_capacity(out_element_size, n);
    if (!mapped) return NULL;

    for (size_t i = 0; i < n; i++) {
        const void* src = __da_index_raw(da, i);
        void* dest = da_index(mapped, i);

        map_fn(dest, src);
    }

    mapped->length = n;

    return mapped;
}

DArray* da_filter(const DArray* da, bool (*filter_fn)(const void* elem)) {
    if (!da || !filter_fn) return NULL;

    DArray* filtered = da_new(da->element_size);
    if (!filtered) return NULL;

    filtered->copier = da->copier;
    filtered->deallocator = da->deallocator;
    filtered->printer = da->printer;

    for (size_t i = 0; i < da->length; i++) {
        const void* elem = __da_index_raw(da, i);
        if (filter_fn(elem)) {
            da_push(filtered, elem);
        }
    }

    return filtered;
}

void da_reduce(const DArray* da, void* acc, void (*reduce_fn)(void* acc, const void* elem)) {
    if (!da || !acc || !reduce_fn) return;

    for (size_t i = 0; i < da->length; i++) {
        const void* elem = __da_index_raw(da, i);
        reduce_fn(acc, elem);
    }
}

/******************************************************************************
 *                                                                            *
 *                       Inner Functions Implementation                       *
 *                                                                            *
 ******************************************************************************/

inline static void __da_default_copier(void* dest, const void* src) {
    (void)dest;
    (void)src;

    fprintf(stderr, "Set copier\n");
    raise(SIGTRAP);
}

inline static void __da_default_deallocator(void* k) {
    (void)k;  // suppress unused warning
}

inline static void __da_default_printer(const void* k) {
    printf("<@%p>", k);
}

inline static const void* __da_index_raw(const DArray* da, size_t idx) {
    return (char*)da->arr + (da->element_size * idx);
}

inline static void __da_set_raw(DArray* da, size_t idx, const void* e) {
    void* dest = da_index(da, idx);
    da->copier(dest, e);
}

inline static bool __da_upsize(DArray* da) {
    if (da->length == da->capacity) {
        const size_t new_cap = da->capacity ? (size_t)((double)da->capacity * da->growth_factor) : 1;
        if (!da_reserve(da, new_cap)) return false;
    }

    return true;
}

inline static void __da_downsize(DArray* da) {
    if (da->length < (size_t)((double)da->capacity * da->shrink_factor)) {
        da_shrink(da);
    }
}

inline static void __da_shift(DArray* da, size_t dest_idx, size_t src_idx) {
    if (!da || dest_idx == src_idx) return;

    // Calculate the number of elements to move
    size_t count = da->length - src_idx;

    const void* src = __da_index_raw(da, src_idx);
    void* dest = da_index(da, dest_idx);

    // memmove handles overlapping memory regions
    memmove(dest, src, count * da->element_size);
}

inline static void __da_swap(void* arr, char* t, size_t element_size, size_t i, size_t j) {
    void* a = (char*)arr + (element_size * i);
    void* b = (char*)arr + (element_size * j);

    // t = *a;
    memcpy(t, a, element_size);
    // *a = *b;
    memcpy(a, b, element_size);
    // *b = t;
    memcpy(b, t, element_size);
}

static void __da_reverse(void* arr, size_t element_size, size_t start, size_t end) {
    if (start >= end) return;

    char* buffer = (char*)malloc(element_size);
    if (!buffer) return;

    // Use the inclusive 'end' index directly
    while (start < end) {
        __da_swap(arr, buffer, element_size, start++, end--);
    }

    free(buffer);
}

#undef DYNAMICARRAY_IMPL