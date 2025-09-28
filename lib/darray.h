#ifndef DYNAMICARRAY_H
#define DYNAMICARRAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// Nomenclature used (to avoid collisions): <data_type>_<method_name>
// Warning: This implementation is not thread-safe, and is for educational purposes only.

/**
 * @brief Dynamic Array Structure (Generic implementation)
 *
 * This structure encapsulates the state and behavior of a generic dynamic array
 * (vector) that can store elements of any single type. It manages resizing,
 * element storage, and custom operations via function pointers.
 *
 * @note CONTRACT: The user is responsible for setting the `copier`,
 * `deallocator`, and `printer` function pointers, especially for complex
 * data types (e.g., structs containing pointers) that require deep copying or
 * specific cleanup. Using the default copier on complex types will result in
 * program termination (`SIGTRAP`) to prevent undefined behavior and memory leaks.
 */
typedef struct DynamicArray DArray;

struct DynamicArray {
    void* arr;            /// Pointer to the underlying heap-allocated array of elements.
    size_t length;        /// The number of elements currently stored in the array `0 <= length <= capacity`.
    size_t capacity;      /// The maximum number of elements the array can hold before reallocation is necessary.
    size_t element_size;  /// The size in bytes of a single element (e.g., `sizeof(int)`).

    double growth_factor;  /// Factor (e.g., 2.0) by which the array's capacity grows when a push operation exceeds the current capacity.
    double shrink_factor;  /// Threshold factor (e.g., 0.2) at which the array should be considered for shrinking to save memory.

    /**
     * @brief Function pointer for copying an element.
     * @details This function is crucial for operations like `da_push`, `da_set`, and `da_copy`.
     * For primitive types, a simple `memcpy` is sufficient. For complex types, it must perform a deep copy.
     * @param dest Pointer to the destination memory block in the array.
     * @param src Pointer to the source memory (element to be copied).
     */
    void (*copier)(void* dest, const void* src);

    /**
     * @brief Function pointer for deallocating an element's internal resources.
     * @details Called by `da_clear`, `da_free`, `da_pop*`, and `da_remove*` to clean up memory *owned by* the element itself (e.g., fields that are heap-allocated pointers).
     * @param k Pointer to the element to be deallocated.
     */
    void (*deallocator)(void* k);

    /**
     * @brief Function pointer for printing an element.
     * @details Used by `da_print` and `da_fprint` to provide custom element representation.
     * @param k Pointer to the element to be printed.
     */
    void (*printer)(FILE* file, const void* k);
};

/******************************************************************************
 *                                                                            *
 *                               Intialization                                *
 *                                                                            *
 ******************************************************************************/

/// @brief Allocates and initializes a new dynamic array.
/// @details The initial capacity is set to a minimum of 4 elements.
/// @param element_size Size of the elements to be stored (e.g., `sizeof(int)`). Must be greater than 0.
/// @return Pointer to the newly constructed `DArray`, or `NULL` on allocation failure.
DArray* da_new(const size_t element_size);

/// @brief Allocates and initializes a new dynamic array with a specified minimum capacity.
/// @param element_size Size of the elements to be stored.
/// @param capacity The initial capacity to reserve for the array.
/// @return Pointer to the newly constructed `DArray`, or `NULL` on allocation failure.
DArray* da_new_with_capacity(const size_t element_size, const size_t capacity);

/// @brief Creates a new dynamic array initialized with a copy of the elements from a raw C array.
/// @param element_size Size of the elements to be stored.
/// @param length The number of elements in the source array.
/// @param arr Pointer to the source C array of elements.
/// @return Pointer to the newly constructed `DArray`, or `NULL` on failure. The resulting array's capacity will be equal to `length`.
DArray* da_new_from_array(const size_t element_size, const size_t length, const void* arr, void (*copier)(void* dest, const void* src));

/// @brief Creates a deep copy of the provided Dynamic Array.
/// @details The elements are copied using the array's defined `copier` function. The new array's capacity will be set to its length.
/// @param da Pointer to the source Dynamic Array.
/// @return Pointer to the newly constructed copy of `DArray`, or `NULL` on failure.
DArray* da_copy(const DArray* da);

/******************************************************************************
 *                                                                            *
 *                             Clean Up & Freeing                             *
 *                                                                            *
 ******************************************************************************/

/// @brief Frees the entire memory associated with the dynamic array.
/// @details This includes deallocating the internal elements via the `deallocator`, freeing the underlying array storage, and finally freeing the `DArray` structure itself.
/// @param da Pointer to the Dynamic Array to free.
void da_free(DArray* da);

/// @brief Clears the array by setting its length to zero and deallocating its elements.
/// @details This operation uses the array's `deallocator` for each element. The array's capacity remains unchanged.
/// @param da Pointer to the dynamic array.
/// @return `true` on success, `false` if `da` is `NULL`.
bool da_clear(DArray* da);

/******************************************************************************
 *                                                                            *
 *                               Basic Getters                                *
 *                                                                            *
 ******************************************************************************/

/// @brief Gets the current number of elements in the dynamic array.
/// @param da Pointer to the Dynamic Array.
/// @return The array's current length.
size_t da_length(const DArray* da);

/// @brief Gets the maximum number of elements the array can currently hold without reallocating.
/// @param da Pointer to the dynamic array.
/// @return The array's current capacity.
size_t da_capacity(const DArray* da);

/// @brief Checks if the dynamic array is empty.
/// @param da Pointer to the dynamic array.
/// @return `true` if `da` is `NULL` or its length is 0, `false` otherwise.
bool da_is_empty(const DArray* da);

/// @brief Provides a mutable pointer to the element at the specified index.
/// @details This function performs **no** `NULL` or bounds checking. Use `da_get` for safe access.
/// @param da Pointer to the dynamic array.
/// @param idx The index of the element to access.
/// @return Pointer to the element at `idx`.
void* da_index(DArray* da, size_t idx);

/******************************************************************************
 *                                                                            *
 *                                  Printing                                  *
 *                                                                            *
 ******************************************************************************/

/// @brief Prints the contents of the dynamic array to `stdout`.
/// @details The format is `[elem1, elem2, ...]`, using the array's defined `printer` function for each element.
/// @param da Pointer to the dynamic array.
void da_print(const DArray* da);

/// @brief Prints the contents of the dynamic array to a specified file stream.
/// @param file Pointer to the output stream (e.g., `stdout` or `stderr`). If `file` is `NULL`, it defaults to `stdout`.
/// @param da Pointer to the dynamic array.
void da_fprint(FILE* file, const DArray* da);

/******************************************************************************
 *                                                                            *
 *                              Advanced Getters                              *
 *                                                                            *
 ******************************************************************************/

/// @brief Returns the raw pointer to the underlying array storage.
/// @details This pointer is still managed by the `DArray` structure. Do not manually free this pointer.
/// @param da Pointer to the dynamic array.
/// @return Pointer to the inner array (`da->arr`).
void* da_raw(DArray* da);

/// @brief Unwraps and returns the raw underlying array pointer, and frees the `DArray` structure itself.
/// @details **The caller is responsible for manually freeing the returned array pointer.** The elements' deallocator is *not* called.
/// @param da Pointer to the dynamic array.
/// @return Pointer to the inner array.
void* da_get_raw(DArray* da);

/// @brief Returns a copy of the elements in the dynamic array as a new raw C array.
/// @details The elements are copied using the array's defined `copier`. **The caller is responsible for manually freeing the returned array pointer.**
/// @param da Pointer to the dynamic array.
/// @return Pointer to a newly allocated C array of elements, or `NULL` on failure.
void* da_get_arr(const DArray* da);

/// @brief Creates a new dynamic array containing a copy of a contiguous subset of elements.
/// @param da Pointer to the dynamic array.
/// @param start The starting index (inclusive).
/// @param end The ending index (exclusive). Must satisfy $start \le end \le \text{da->length}$.
/// @return Pointer to the newly created subarray `DArray`, or `NULL` on error (e.g., invalid indices or allocation failure).
DArray* da_get_subarr(const DArray* da, size_t start, size_t end);

/// @brief Safely retrieves a pointer to the element at the specified index.
/// @details Performs `NULL` and bounds checks.
/// @param da Pointer to the dynamic array.
/// @param idx Index of the element to retrieve. Must be $0 \le \text{idx} < \text{da->length}$.
/// @return Pointer to the element at `idx`, or `NULL` if `da` is `NULL` or `idx` is out of bounds.
void* da_get(DArray* da, size_t idx);

/// @brief Safely retrieves a pointer to the first element ($idx = 0$).
/// @param da Pointer to the dynamic array.
/// @return Pointer to the first element, or `NULL` if the array is empty or `NULL`.
void* da_get_first(DArray* da);

/// @brief Safely retrieves a pointer to the last element ($idx = \text{length}-1$).
/// @param da Pointer to the dynamic array.
/// @return Pointer to the last element, or `NULL` if the array is empty or `NULL`.
void* da_get_last(DArray* da);

/******************************************************************************
 *                                                                            *
 *                                 Searching                                  *
 *                                                                            *
 ******************************************************************************/

/// @brief Performs a linear search for the first occurrence of a target element.
/// @param da Pointer to the dynamic array.
/// @param target Pointer to the target element to search for.
/// @param cmp Pointer to the comparator function: returns 0 if elements are equal.
/// @return The index of the first matching element, or `(size_t)-1` on error or if the element is not found.
size_t da_find(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b));

/// @brief Performs a binary search for an element in a **sorted** dynamic array.
/// @details **The caller is responsible for ensuring the array is sorted** according to the `cmp` function.
/// @param da Pointer to the dynamic array.
/// @param target Pointer to the target element to search for.
/// @param cmp Pointer to the comparator function: returns 0 if elements are equal.
/// @return The index of the found element, or `(size_t)-1` on error or if the element is not found.
size_t da_binary_search(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b));

/// @brief Checks for the presence of a target element using linear search.
/// @param da Pointer to the dynamic array.
/// @param target Pointer to the target element.
/// @param cmp Pointer to the comparator function.
/// @return `true` if found, `false` otherwise (including on error).
bool da_contains(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b));

/// @brief Checks for the presence of a target element using C's standard library `bsearch`.
/// @details **The caller is responsible for ensuring the array is sorted.**
/// @param da Pointer to the dynamic array.
/// @param target Pointer to the target element.
/// @param cmp Pointer to the comparator function.
/// @return `true` if found, `false` otherwise (including on error).
bool da_contains_bsearch(const DArray* da, const void* target, int (*cmp)(const void* a, const void* b));

/******************************************************************************
 *                                                                            *
 *                                  Compare                                   *
 *                                                                            *
 ******************************************************************************/

/// @brief Checks whether the two arrays are equal ot not.
/// @param a Pointer to the dynamic array.
/// @param b Pointer to the dynamic array.
/// @return `true` if equal, `false` otherwise (including on error).
bool da_are_eq(const DArray* a, const DArray* b, int (*cmp)(const void* a, const void* b));

/******************************************************************************
 *                                                                            *
 *                                  Setters                                   *
 *                                                                            *
 ******************************************************************************/

/// @brief Writes a copy of the provided element at the specified index.
/// @details The index `idx` can range from $0$ up to $\text{capacity}-1$. If `idx` is greater than or equal to the current length, the length is extended to `idx + 1`. The elements between the original length and `idx` are left uninitialized (creating a "hole").
/// @param da Pointer to the dynamic array.
/// @param idx The index at which to write the element.
/// @param e Pointer to the source element.
/// @return `true` on success, `false` on error (e.g., `da` is `NULL`, `idx` is out of capacity bounds, or `e` is `NULL`).
bool da_set(DArray* da, size_t idx, const void* e);

/// @brief Swaps the elements located at index `i` and index `j`.
/// @param da Pointer to the dynamic array.
/// @param i First index. Must be $0 \le i < \text{length}$.
/// @param j Second index. Must be $0 \le j < \text{length}$.
/// @return `true` on success, `false` on error (e.g., out of bounds or memory allocation failure for temporary buffer).
bool da_swap(DArray* da, size_t i, size_t j);

/******************************************************************************
 *                                                                            *
 *                            Insertion & Deletion                            *
 *                                                                            *
 ******************************************************************************/

/// @brief Appends a copy of the provided element to the end of the array ($\text{idx} = \text{length}$).
/// @details This operation may trigger a resize if $\text{length} == \text{capacity}$.
/// @param da Pointer to the dynamic array.
/// @param e Pointer to the source element.
/// @return `true` on success, `false` on failure (e.g., allocation failure during resize).
bool da_push(DArray* da, const void* e);

/// @brief Inserts a copy of the provided element at the beginning of the array ($\text{idx} = 0$).
/// @details This operation involves shifting all existing elements and may trigger a resize.
/// @param da Pointer to the dynamic array.
/// @param e Pointer to the source element.
/// @return `true` on success, `false` on failure (e.g., allocation failure during resize).
bool da_push_front(DArray* da, const void* e);

/// @brief Removes the last element from the array, deallocates its original in-place storage, and returns a copy of the element.
/// @details This operation may trigger a shrink-to-fit resize if the length drops below the shrink factor threshold. **The caller is responsible for manually freeing the memory of the returned element copy.**
/// @param da Pointer to the dynamic array.
/// @return A pointer to the popped element's copy, or `NULL` if the array is empty or on memory allocation failure.
void* da_pop(DArray* da);

/// @brief Removes the first element from the array, deallocates its original in-place storage, and returns a copy of the element.
/// @details This operation involves shifting all remaining elements and may trigger a shrink-to-fit resize. **The caller is responsible for manually freeing the memory of the returned element copy.**
/// @param da Pointer to the dynamic array.
/// @return A pointer to the popped element's copy, or `NULL` if the array is empty or on memory allocation failure.
void* da_pop_front(DArray* da);

/// @brief Searches for and removes the first occurrence of the `target` element.
/// @details The original element's in-place storage is deallocated using `da->deallocator`. Subsequent elements are shifted to fill the gap.
/// @param da Pointer to the dynamic array.
/// @param target Pointer to the target element.
/// @param cmp Pointer to the comparator function.
/// @return The index of the removed element, or `(size_t)-1` if the element is not found or on error.
size_t da_remove(DArray* da, void* target, int (*cmp)(const void* a, const void* b));

/// @brief Inserts a copy of the provided element at the specified index.
/// @details All elements from `idx` onwards are shifted to the right. The index `idx` must be in the range $[0, \text{length}]$.
/// @param da Pointer to the dynamic array.
/// @param idx Index at which to insert. If $\text{idx} = 0$, it calls `da_push_front`. If $\text{idx} = \text{length}$, it calls `da_push`.
/// @param e Pointer to the source element.
/// @return `true` on success, `false` on failure (e.g., invalid index or allocation failure).
bool da_insert_at(DArray* da, size_t idx, const void* e);

/// @brief Removes the element at the provided index.
/// @details The element's original in-place storage is deallocated using `da->deallocator`. Subsequent elements are shifted to the left. The index `idx` must be in the range $[0, \text{length}-1]$.
/// @param da Pointer to the dynamic array.
/// @param idx Index of the element to remove.
/// @return `true` on success, `false` on failure (e.g., invalid index).
bool da_remove_at(DArray* da, size_t idx);

/******************************************************************************
 *                                                                            *
 *                                  Resizing                                  *
 *                                                                            *
 ******************************************************************************/

/// @brief Reduces the array's length to a new value, deallocating the excess elements.
/// @details Elements from `new_length` to $\text{length}-1$ are deallocated using `da->deallocator`.
/// @param da Pointer to the dynamic array.
/// @param new_length The desired final length. If `new_length` is greater than or equal to the current length, no action is taken besides returning `true`.
/// @return `true` on success, `false` if `da` is `NULL`.
bool da_truncate(DArray* da, size_t new_length);

/// @brief Resizes the underlying array storage to exactly the given capacity.
/// @details If the new capacity is less than the current length, the array is first truncated, and excess elements are deallocated.
/// @param da Pointer to the dynamic array.
/// @param capacity The new storage capacity.
/// @return `true` on success, `false` on reallocation failure if `capacity > 0`.
bool da_resize(DArray* da, size_t capacity);

/// @brief Ensures the array has at least the given capacity.
/// @details If the current capacity is less than the requested capacity, it resizes the array. Otherwise, it does nothing.
/// @param da Pointer to the dynamic array.
/// @param capacity The minimum required capacity.
/// @return `true` on success, `false` on reallocation failure.
bool da_reserve(DArray* da, size_t capacity);

/// @brief Resizes the array to exactly fit its current length ($\text{capacity} = \text{length}$).
/// @param da Pointer to the dynamic array.
/// @return `true` on success, `false` on reallocation failure.
bool da_shrink(DArray* da);

/******************************************************************************
 *                                                                            *
 *                               Concatanation                                *
 *                                                                            *
 ******************************************************************************/

/// @brief Concatenates two dynamic arrays into a new dynamic array.
/// @details Requires both arrays to have the same `element_size`. If one is `NULL`, a copy of the other is returned. Elements are copied using array `a`'s `copier`.
/// @param a Pointer to the first dynamic array.
/// @param b Pointer to the second dynamic array.
/// @return Pointer to the newly concatenated array, or `NULL` if element sizes mismatch or on allocation failure.
DArray* da_concat(const DArray* a, const DArray* b);

/// @brief Merges two **sorted** dynamic arrays into a new sorted dynamic array.
/// @details **The caller is responsible for ensuring both arrays are sorted.** Requires both arrays to have the same `element_size`. Elements are copied using array `a`'s `copier`.
/// @param a Pointer to the first sorted dynamic array.
/// @param b Pointer to the second sorted dynamic array.
/// @param cmp Pointer to the comparator function used for merging.
/// @return Pointer to the newly merged sorted array, or `NULL` on error (e.g., mismatching element sizes, `cmp` is `NULL`, or allocation failure).
DArray* da_merge_sorted(const DArray* a, const DArray* b, int (*cmp)(const void* a, const void* b));

/******************************************************************************
 *                                                                            *
 *                        Sorting & Order Manipulation                        *
 *                                                                            *
 ******************************************************************************/

/// @brief Sorts the elements of the dynamic array in-place.
/// @details Uses the standard library's `qsort` implementation.
/// @param da Pointer to the dynamic array.
/// @param cmp Pointer to the comparison function for sorting.
void da_sort(DArray* da, int (*cmp)(const void* a, const void* b));

/// @brief Reverses the order of elements in the dynamic array in-place.
/// @param da Pointer to the dynamic array.
void da_reverse(DArray* da);

/// @brief Rotates the elements of the dynamic array `k` steps to the left (counter-clockwise) in-place.
/// @param da Pointer to the dynamic array.
/// @param k The number of steps to rotate. Rotation is performed modulo $\text{length}$.
void da_rotate_left(DArray* da, size_t k);

/// @brief Rotates the elements of the dynamic array `k` steps to the right (clockwise) in-place.
/// @param da Pointer to the dynamic array.
/// @param k The number of steps to rotate. Rotation is performed modulo $\text{length}$.
void da_rotate_right(DArray* da, size_t k);

/******************************************************************************
 *                                                                            *
 *                             Functional Methods                             *
 *                                                                            *
 ******************************************************************************/

/// @brief Creates a new dynamic array by applying a transformation function to every element of the source array.
/// @details The output array can hold elements of a different size than the input array.
/// @param da Pointer to the source dynamic array.
/// @param map_fn Pointer to the transformation function: `map_fn(dest, src)`.
/// @param out_element_size The size of the elements in the resulting mapped array.
/// @return Pointer to the newly mapped array, or `NULL` on error (e.g., `da` or `map_fn` is `NULL`, or allocation failure).
DArray* da_map(const DArray* da, void (*map_fn)(void* dest, const void* src), const size_t out_element_size);

/// @brief Creates a new dynamic array containing only the elements that satisfy a given condition.
/// @details Elements that pass the filter are deep-copied into the new array using the source array's `copier`. The new array has the same `element_size` as the source.
/// @param da Pointer to the source dynamic array.
/// @param filter_fn Pointer to the predicate function: returns `true` to keep the element.
/// @return Pointer to the newly filtered array, or `NULL` on error.
DArray* da_filter(const DArray* da, bool (*filter_fn)(const void* elem));

/// @brief Reduces the dynamic array to a single value by applying an operation iteratively.
/// @details The `acc` value is updated in place. This is equivalent to an in-place *fold* operation.
/// @param da Pointer to the source dynamic array.
/// @param acc Pointer to the accumulator variable (stores the final reduced result).
/// @param reduce_fn Pointer to the reduction function: `reduce_fn(acc, elem)`.
void da_reduce(const DArray* da, void* acc, void (*reduce_fn)(void* acc, const void* elem));

/**
 * @brief Dynamic Array Iterator Structure (Generic implementation)
 *
 * This structure encapsulates the state and of the iterator.
 *
 * @note CONTRACT: The use shall not manipulate the Array which is being iterated.
 */
typedef struct DynamicArrayIterator DAIterator;

struct DynamicArrayIterator {
    void* arr;            /// Pointer to the inner array of the `DArray`.
    size_t index;         /// Index to the current element.
    size_t length;        /// Length of the array.
    size_t element_size;  /// Size of the elements.
};

/// @brief Creates and initializes a new iterator for the given dynamic array.
/// @param da Pointer to the dynamic array to iterate over.
/// @return Pointer to the newly allocated DAIterator, or NULL on allocation failure.
DAIterator* da_iterator(DArray* da);

/// @brief Advances the iterator to the next element.
/// @details Increments the internal index.
/// @param dai Pointer to the DAIterator.
/// @return `true` if there is a next element, `false` otherwise (end of array).
bool dai_next(DAIterator* dai);

// @brief Retrieves a pointer to the current element in the iteration.
// @details Returns a pointer to the element at `dai->index - 1`.
// @param dai Pointer to the DAIterator.
// @return Pointer to the current element, or NULL if the iterator is not yet advanced or has finished.
void* dai_get(DAIterator* dai);

/// @brief Frees the memory associated with the iterator structure.
/// @param dai Pointer to the DAIterator to free.
void dai_free(DAIterator* dai);

#endif  // DYNAMICARRAY_H