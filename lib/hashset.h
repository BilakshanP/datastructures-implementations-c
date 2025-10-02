#ifndef HASHSET_H
#define HASHSET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Nomenclature used (to avoid collisions): <data_type>_<method_name>
// Warning: This implementation is not thread-safe, and is for educational purposes only.

/**
 * @brief Opaque structure for the Generic Hash Set.
 *
 * This structure encapsulates the state and behavior of a generic HashSet.
 * The internal fields can be modified (e.g., seeds, factors, printer)
 * as long as the set is empty, but generally should be accessed through
 * provided functions.
 */
typedef struct HashSet HSet;

/**
 * @brief Hash Set's underlying node structure for separate chaining.
 */
typedef struct HashSetNode HSNode;

/**
 * @brief Internal structure for a single node in the linked list (bucket).
 */
struct HashSetNode {
    void* key;      ///< Pointer to the key data.
    uint64_t hash;  ///< Cached hash of the key.
    HSNode* next;   ///< Pointer to the next node in the bucket's linked list.
};

/**
 * @brief The main Hash Set structure definition.
 */
struct HashSet {
    HSNode** buckets;  ///< An array of pointers to `HSNode`'s (the hash table).

    size_t count;         ///< The current number of elements stored.
    size_t capacity;      ///< The length of the `buckets` array.
    size_t element_size;  ///< Size of key type in bytes (used for memory allocation).

    uint64_t seed_0;  ///< First seed for the inner hashing function.
    uint64_t seed_1;  ///< Second seed for the inner hashing function.

    double load_factor;    ///< Usage (`count/capacity`) threshold for resizing.
    double growth_factor;  ///< Factor by which capacity grows when resizing.

    /**
     * @brief Pointer to the comparator function.
     * @details Must return 0 if elements are equal.
     */
    int (*cmp)(const void* a, const void* b);

    /**
     * @brief Pointer to the hashing function.
     * @details Takes the key and the two internal seeds as input.
     */
    uint64_t (*hasher)(const void* k, uint64_t seed_0, uint64_t seed_1);

    /**
     * @brief Pointer to the deep copy function.
     * @details Used when inserting a new element. Copies `src` data to `dest` memory.
     */
    void (*copier)(void* dest, const void* src);

    /**
     * @brief Pointer to the deallocation function.
     * @details Used when removing or clearing the set. Frees any internal memory held by the key.
     */
    void (*deallocator)(void* k);

    /**
     * @brief Pointer to the printer function (optional).
     * @details Used for printing the set contents. If NULL, a generic pointer/hash is printed.
     */
    void (*printer)(FILE* file, const void* k);

    uint64_t _mut_count;   ///< Mutation count, incremented on any attempt to change the set structure.
    uint64_t _collisions;  ///< Count of collisions detected during insertion.
};

/******************************************************************************
 *                                                                            *
 *                               Intialization                                *
 *                                                                            *
 ******************************************************************************/

/// @brief Creates a new Hash Set with a default initial capacity (4).
///
/// @param element_size The size of the key type in bytes (e.g., `sizeof(int)`).
/// @param cmp Pointer to the comparison function.
/// @param hasher Pointer to the hashing function.
/// @param copier Pointer to the deep copy function.
/// @param deallocator Pointer to the deallocation function.
/// @return A pointer to the newly allocated HSet, or NULL on failure.
HSet* hs_new(
    size_t element_size,
    int (*cmp)(const void* a, const void* b),
    uint64_t (*hasher)(const void* k, uint64_t seed_0, uint64_t seed_1),
    void (*copier)(void* dest, const void* src),
    void (*deallocator)(void* k)  //
);

/// @brief Creates a new Hash Set with a specified capacity.
///
/// The capacity will be rounded up to the next power of 2 (minimum 4).
///
/// @param element_size The size of the key type in bytes.
/// @param cmp Pointer to the comparison function.
/// @param hasher Pointer to the hashing function.
/// @param copier Pointer to the deep copy function.
/// @param deallocator Pointer to the deallocation function.
/// @param capacity The desired minimum capacity.
/// @return A pointer to the newly allocated HSet, or NULL on failure.
HSet* hs_new_with_capacity(
    size_t element_size,
    int (*cmp)(const void* a, const void* b),
    uint64_t (*hasher)(const void* k, uint64_t seed_0, uint64_t seed_1),
    void (*copier)(void* dest, const void* src),
    void (*deallocator)(void* k),
    size_t capacity  //
);

/// @brief Creates a new Hash Set and populates it with unique elements from an array.
///
/// @param element_size The size of the key type in bytes.
/// @param cmp Pointer to the comparison function.
/// @param hasher Pointer to the hashing function.
/// @param copier Pointer to the deep copy function.
/// @param deallocator Pointer to the deallocation function.
/// @param arr Pointer to the contiguous array of elements.
/// @param length The number of elements in the array.
/// @return A pointer to the newly allocated HSet, or NULL on failure.
HSet* hs_new_from_array(
    size_t element_size,
    int (*cmp)(const void* a, const void* b),
    uint64_t (*hasher)(const void* k, uint64_t seed_0, uint64_t seed_1),
    void (*copier)(void* dest, const void* src),
    void (*deallocator)(void* k),
    const void* arr,
    size_t length  //
);

/******************************************************************************
 *                                                                            *
 *                             Clean Up & Freeing                             *
 *                                                                            *
 ******************************************************************************/

/// @brief Frees the entire Hash Set and all its contained elements.
///
/// Calls the `deallocator` on every element's key before freeing the set structure itself.
///
/// @param hs Pointer to the Hash Set to free.
void hs_free(HSet* hs);

/// @brief Removes all elements from the Hash Set but keeps the structure allocated.
///
/// Calls the `deallocator` on every element's key. The capacity remains the same.
///
/// @param hs Pointer to the Hash Set to clear.
void hs_clear(HSet* hs);

/******************************************************************************
 *                                                                            *
 *                               Basic Getters                                *
 *                                                                            *
 ******************************************************************************/

/// @brief Gets the current number of elements in the Hash Set.
///
/// @param hs Pointer to the Hash Set.
/// @return The count of elements.
size_t hs_count(const HSet* hs);

/// @brief Checks if the Hash Set is empty.
///
/// @param hs Pointer to the Hash Set.
/// @return Non-zero (true) if empty, zero (false) otherwise.
size_t hs_is_empty(const HSet* hs);

/******************************************************************************
 *                                                                            *
 *                                  Copiers                                   *
 *                                                                            *
 ******************************************************************************/

/// @brief Performs a deep copy of the Hash Set.
///
/// Copies all elements and metadata.
///
/// @param hs Pointer to the source Hash Set.
/// @return A pointer to the new, copied HSet, or NULL on failure.
HSet* hs_copy(const HSet* hs);

/// @brief Performs a deep copy of the Hash Set with a specified minimum capacity.
///
/// Copies all elements and metadata.
///
/// @param hs Pointer to the source Hash Set.
/// @param capacity The minimum capacity for the new set.
/// @return A pointer to the new, copied HSet, or NULL on failure.
HSet* hs_copy_with_capacity(const HSet* hs, size_t capacity);

/// @brief Creates a new, empty Hash Set, copying only the metadata (functions, factors, seeds).
///
/// @param hs Pointer to the source Hash Set.
/// @return A pointer to the new HSet with metadata copied, or NULL on failure.
HSet* hs_copy_metadata(const HSet* hs);

/// @brief Creates a new, empty Hash Set, copying only the metadata and using a specified capacity.
///
/// @param hs Pointer to the source Hash Set.
/// @param capacity The desired minimum capacity.
/// @return A pointer to the new HSet with metadata copied, or NULL on failure.
HSet* hs_copy_metadata_with_capacity(const HSet* hs, size_t capacity);

/******************************************************************************
 *                                                                            *
 *                                  Printing                                  *
 *                                                                            *
 ******************************************************************************/

/// @brief Prints the contents of the Hash Set to standard output (`stdout`).
///
/// @param hs Pointer to the Hash Set.
void hs_print(const HSet* hs);

/// @brief Prints the contents of the Hash Set to a specified file stream.
///
/// Uses the custom `printer` function if available.
///
/// @param file The file stream to print to.
/// @param hs Pointer to the Hash Set.
void hs_fprint(FILE* file, const HSet* hs);

/// @brief Prints the debug contents of the Hash Set to a specified file stream.
///
/// Shows elements organized by bucket index.
///
/// @param file The file stream to print to.
/// @param hs Pointer to the Hash Set.
void hs_fprint_debug(FILE* file, const HSet* hs);

/// @brief Prints the metadata and statistics of the Hash Set to a specified file stream.
///
/// Includes count, capacity, usage, seeds, and collision count.
///
/// @param file The file stream to print to.
/// @param hs Pointer to the Hash Set.
void hs_fprint_metadata(FILE* file, const HSet* hs);

/******************************************************************************
 *                                                                            *
 *                                   Resize                                   *
 *                                                                            *
 ******************************************************************************/

/// @brief Manually resizes the Hash Set capacity.
///
/// Capacity is only increased (if `new_capacity > current_capacity`).
///
/// @param hs Pointer to the Hash Set.
/// @param new_capacity The new minimum capacity to resize to.
/// @return true if resizing occurred or if the new capacity was smaller than the old one, false on allocation failure.
bool hs_resize(HSet* hs, size_t new_capacity);

/******************************************************************************
 *                                                                            *
 *                      Insertion, Deletion & Searching                       *
 *                                                                            *
 ******************************************************************************/

/// @brief Inserts a key into the Hash Set.
///
/// A deep copy of the key is made using the `copier` function.
/// If the load factor is exceeded, the set will automatically resize.
///
/// @param hs Pointer to the Hash Set.
/// @param k Pointer to the key data to insert.
/// @return true if the key was successfully inserted (was not already present), false otherwise.
bool hs_insert(HSet* hs, const void* k);

/// @brief Removes a key from the Hash Set.
///
/// The element's memory is freed using the `deallocator` function.
///
/// @param hs Pointer to the Hash Set.
/// @param k Pointer to the key data to remove.
/// @return true if the key was found and removed, false otherwise.
bool hs_remove(HSet* hs, const void* k);

/// @brief Retains only the elements in the set for which the predicate returns true.
///
/// Elements for which the predicate returns false are removed and deallocated.
///
/// @param hs Pointer to the Hash Set.
/// @param predicate A function that returns true for elements to keep.
void hs_retain(HSet* hs, bool (*predicate)(void* k));

/// @brief Checks if a key is present in the Hash Set.
///
/// @param hs Pointer to the Hash Set.
/// @param k Pointer to the key data to check.
/// @return true if the key is found, false otherwise.
bool hs_contains(const HSet* hs, const void* k);

/******************************************************************************
 *                                                                            *
 *                              Advanced Getters                              *
 *                                                                            *
 ******************************************************************************/

/// @brief Extracts all elements from the set into a new contiguous array.
///
/// The returned array is a deep copy of the keys.
///
/// @param hs Pointer to the Hash Set.
/// @return A pointer to the newly allocated array of keys, or NULL on failure. The caller must free this array.
void* hs_extract(const HSet* hs);

/******************************************************************************
 *                                                                            *
 *                                Comparators                                 *
 *                                                                            *
 ******************************************************************************/

/// @brief Checks if two Hash Sets are equal ($A = B$).
///
/// @param a Pointer to the first Hash Set.
/// @param b Pointer to the second Hash Set.
/// @return true if both sets contain the same elements, false otherwise.
bool hs_are_eq(const HSet* a, const HSet* b);

/// @brief Checks if two Hash Sets are disjoint ($A \cap B = \emptyset$).
///
/// @param a Pointer to the first Hash Set.
/// @param b Pointer to the second Hash Set.
/// @return true if the sets have no common elements, false otherwise.
bool hs_are_disjoint(const HSet* a, const HSet* b);

/// @brief Checks if set A is a subset of set B ($A \subseteq B$).
///
/// @param a Pointer to the potential subset.
/// @param b Pointer to the potential superset.
/// @return true if every element in A is also in B, false otherwise.
bool hs_is_subset(const HSet* a, const HSet* b);

/// @brief Checks if set A is a superset of set B ($A \supseteq B$).
///
/// @param a Pointer to the potential superset.
/// @param b Pointer to the potential subset.
/// @return true if every element in B is also in A, false otherwise.
bool hs_is_supset(const HSet* a, const HSet* b);

/******************************************************************************
 *                                                                            *
 *                            Algebric Operations                             *
 *                                                                            *
 ******************************************************************************/

/// @brief Computes the union of two sets ($A \cup B$).
///
/// Returns a new Hash Set containing all elements from both A and B.
///
/// @param a Pointer to the first Hash Set.
/// @param b Pointer to the second Hash Set.
/// @return A new HSet representing the union, or NULL on failure.
HSet* hs_union(const HSet* a, const HSet* b);

/// @brief Computes the intersection of two sets ($A \cap B$).
///
/// Returns a new Hash Set containing only elements common to both A and B.
///
/// @param a Pointer to the first Hash Set.
/// @param b Pointer to the second Hash Set.
/// @return A new HSet representing the intersection, or NULL on failure.
HSet* hs_intersection(const HSet* a, const HSet* b);

/// @brief Computes the difference of two sets ($A \setminus B$).
///
/// Returns a new Hash Set containing elements in A but not in B.
///
/// @param a Pointer to the minuend set.
/// @param b Pointer to the subtrahend set.
/// @return A new HSet representing the difference, or NULL on failure.
HSet* hs_difference(const HSet* a, const HSet* b);

/// @brief Computes the symmetric difference of two sets ($A \Delta B$).
///
/// Returns a new Hash Set containing elements in A or B, but not both.
///
/// @param a Pointer to the first Hash Set.
/// @param b Pointer to the second Hash Set.
/// @return A new HSet representing the symmetric difference, or NULL on failure.
HSet* hs_sym_difference(const HSet* a, const HSet* b);

/// @brief Creates a new Hash Set containing elements that satisfy a predicate.
///
/// @param hs Pointer to the source Hash Set.
/// @param predicate A function that returns true for elements to include.
/// @return A new HSet containing the filtered elements, or NULL on failure.
HSet* hs_filter(const HSet* hs, bool (*predicate)(void* k));

/******************************************************************************
 *                                                                            *
 *                                  Iterator                                  *
 *                                                                            *
 ******************************************************************************/

/// @brief Hash Set Iterator Struct for traversing elements.
///
/// Iterators are invalidated if the underlying HSet is modified during iteration.
typedef struct {
    HSet* hs;            ///< The hashset being iterated.
    size_t index;        ///< Current bucket index.
    HSNode* node;        ///< Current entry within the bucket.
    uint64_t mutations;  ///< Snapshot of the set's mutation count for safety checks.
} HSIterator;

/// @brief Initializes an iterator for the given Hash Set.
///
/// The returned iterator must be freed by the user after use.
///
/// @param hs Pointer to the Hash Set.
/// @return A pointer to the newly allocated HSIterator, positioned before the first element, or NULL on failure.
HSIterator* hs_iterator(HSet* hs);

/// @brief Advances the iterator to the next element.
///
/// @param it Pointer to the iterator.
/// @return true if advanced to a valid element, false if the end is reached or the set was mutated.
bool hs_iter_next(HSIterator* it);

/// @brief Gets the current element's key data from the iterator.
///
/// @param it Pointer to the iterator.
/// @return Pointer to the current element's key data, or NULL if invalid or if the set was mutated.
void* hs_iter_get(HSIterator* it);

#endif  // HASHSET_H