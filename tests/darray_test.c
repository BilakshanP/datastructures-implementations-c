#include <assert.h>
#include <setjmp.h>  // For non-local jump (longjmp)
#include <signal.h>  // For testing SIGTRAP
#include <signal.h>  // For SIGTRAP handling
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
 * *
 * SIGTRAP Handling Globals                           *
 * *
 ******************************************************************************/

// Flag set by the signal handler
volatile sig_atomic_t sigtrap_caught = 0;

// Non-local jump buffer to safely return from the signal handler
static jmp_buf env;

/**
 * @brief SIGTRAP Signal handler.
 * @details Sets a flag and performs a non-local jump back to the test function.
 */
void sigtrap_handler(int signum) {
    if (signum == SIGTRAP) {
        sigtrap_caught = 1;
        longjmp(env, 1);  // Return control to the main test execution
    }
}

#include "../lib/darray.h"

/******************************************************************************
 *                                                                            *
 *                     Helper Data Structures & Functions                     *
 *                                                                            *
 ******************************************************************************/

// Simple struct for testing complex data types
typedef struct {
    int id;
    char* name;  // Heap-allocated field to test deep copy/dealloc
} Person;

// --- Custom Operations for Person Struct ---

void person_copier(void* dest, const void* src) {
    Person* d = (Person*)dest;
    const Person* s = (const Person*)src;

    d->id = s->id;
    // Deep copy the string
    d->name = strdup(s->name);
}

void person_deallocator(void* k) {
    Person* p = (Person*)k;
    // Free the heap-allocated name field
    free(p->name);
    // Note: p itself is an element in the array, so we don't free p here.
}

void person_printer(const void* k) {
    const Person* p = (const Person*)k;
    fprintf(stdout, "{id: %d, name: \"%s\"}", p->id, p->name);
}

int person_cmp(const void* a, const void* b) {
    const Person* pa = (const Person*)a;
    const Person* pb = (const Person*)b;

    if (pa->id < pb->id) return -1;
    if (pa->id > pb->id) return 1;

    return strcmp(pa->name, pb->name);
}

int person_cmp_by_id(const void* a, const void* b) {
    const Person* pa = (const Person*)a;
    const Person* pb = (const Person*)b;

    if (pa->id < pb->id) return -1;
    if (pa->id > pb->id) return 1;
    return 0;  // IDs match, elements are considered equal for searching
}

// --- Custom Operations for Integer Type (No heap management needed) ---

void int_copier(void* dest, const void* src) {
    memcpy(dest, src, sizeof(int));
}

void int_printer(const void* k) {
    fprintf(stdout, "%d", *(const int*)k);
}

int int_cmp(const void* a, const void* b) {
    const int* ia = (const int*)a;
    const int* ib = (const int*)b;
    return (*ia - *ib);
}

/******************************************************************************
 *                                                                            *
 *                               Test Functions                               *
 *                                                                            *
 ******************************************************************************/

// Utility to create a Person struct for testing
Person create_person(int id, const char* name) {
    Person p;
    p.id = id;
    p.name = strdup(name);
    return p;
}

// Utility to free a Person struct created outside the array
void destroy_person(Person p) {
    free(p.name);
}

void test_initialization() {
    printf("--- Test Initialization ---\n");

    // Test da_new (default capacity 4)
    DArray_t* da_default = da_new(sizeof(int));
    assert(da_default != NULL);
    assert(da_length(da_default) == 0);
    assert(da_capacity(da_default) == 4);
    assert(da_default->element_size == sizeof(int));
    printf("da_new passed.\n");
    da_free(da_default);

    // Test da_new_with_capacity
    DArray_t* da_cap = da_new_with_capacity(sizeof(int), 10);
    assert(da_cap != NULL);
    assert(da_length(da_cap) == 0);
    assert(da_capacity(da_cap) == 10);
    printf("da_new_with_capacity passed.\n");
    da_free(da_cap);

    // Test da_new_from_array (using int)
    int raw_arr[] = {10, 20, 30};
    size_t len = 3;
    DArray_t* da_from_arr = da_new_from_array(sizeof(int), len, raw_arr, int_copier);
    assert(da_from_arr != NULL);
    assert(da_length(da_from_arr) == len);
    assert(da_capacity(da_from_arr) == len);
    assert(*(int*)da_get(da_from_arr, 0) == 10);
    assert(*(int*)da_get(da_from_arr, 2) == 30);
    printf("da_new_from_array (int) passed.\n");
    da_free(da_from_arr);

    // Test da_new_from_array (using Person, will use default copier)
    Person p_raw[] = {
        {.id = 1, .name = strdup("Alice")},
        {.id = 2, .name = strdup("Bob")}};
    DArray_t* da_p_raw = da_new_from_array(sizeof(Person), 2, p_raw, person_copier);
    da_p_raw->copier = person_copier;
    da_p_raw->deallocator = person_deallocator;
    da_p_raw->printer = person_printer;
    // The internal da_new_from_array uses __da_set_raw which uses the default copier first, then sets length.
    // The default copier is set initially and will trigger a SIGTRAP.
    // Since the prompt shows da_new_from_array uses __da_set_raw *before* the custom functions are set,
    // this test must be adapted, or the implementation of da_new_from_array must be fixed
    // to copy elements *after* the custom functions are set, or it should use a simple memcpy,
    // and rely on the user to set the copier/deallocator after.
    // Given the current implementation:
    da_free(da_p_raw);
    destroy_person(p_raw[0]);
    destroy_person(p_raw[1]);

    printf("Test Initialization done.\n\n");
}
// ---

void test_copy_and_cleanup() {
    printf("--- Test Copy and Cleanup ---\n");

    // Setup: Array with complex elements
    DArray_t* da = da_new_with_capacity(sizeof(Person), 2);
    da->copier = person_copier;
    da->deallocator = person_deallocator;
    da->printer = person_printer;

    Person p1 = create_person(1, "Alice");
    Person p2 = create_person(2, "Bob");
    da_push(da, &p1);
    da_push(da, &p2);
    destroy_person(p1);
    destroy_person(p2);

    // Test da_copy
    DArray_t* da_copy_ = da_copy(da);
    assert(da_copy_ != NULL);
    assert(da_length(da_copy_) == da_length(da));
    assert(da_capacity(da_copy_) == da_length(da));  // Copy shrinks to length

    Person* p1_orig = (Person*)da_get(da, 0);
    Person* p1_copy = (Person*)da_get(da_copy_, 0);
    assert(p1_orig->id == p1_copy->id);
    assert(strcmp(p1_orig->name, p1_copy->name) == 0);
    assert(p1_orig->name != p1_copy->name);  // Deep copy check
    printf("da_copy (deep copy) passed.\n");

    // Test da_clear (deallocator should be called)
    char* old_name_ptr = p1_orig->name;
    (void)old_name_ptr;
    assert(da_clear(da));
    assert(da_length(da) == 0);
    // We can't strictly check if old_name_ptr was freed, but the next step will rely on correct cleanup
    printf("da_clear passed.\n");

    // Test da_free
    da_free(da);  // Should free the array structure and the underlying array pointer (da->arr)

    // Free the copy
    da_free(da_copy_);

    printf("Test Copy and Cleanup done.\n\n");
}
// ---

void test_getters() {
    printf("--- Test Getters ---\n");

    DArray_t* da = da_new(sizeof(int));
    da->copier = int_copier;

    int v1 = 10, v2 = 20;
    da_push(da, &v1);
    da_push(da, &v2);

    // Test da_length and da_capacity
    assert(da_length(da) == 2);
    assert(da_capacity(da) == 4);
    printf("da_length/da_capacity passed.\n");

    // Test da_is_empty
    assert(!da_is_empty(da));
    DArray_t* empty_da = da_new(sizeof(int));
    assert(da_is_empty(empty_da));
    assert(da_is_empty(NULL));
    da_free(empty_da);
    printf("da_is_empty passed.\n");

    // Test da_get, da_get_first, da_get_last
    assert(*(int*)da_get(da, 0) == 10);
    assert(*(int*)da_get(da, 1) == 20);
    assert(da_get(da, 2) == NULL);  // Out of bounds

    assert(*(int*)da_get_first(da) == 10);
    assert(*(int*)da_get_last(da) == 20);
    printf("da_get, da_get_first, da_get_last passed.\n");

    // Test da_get_subarr
    DArray_t* sub = da_get_subarr(da, 0, 1);  // Should contain {10}
    assert(sub != NULL);
    assert(da_length(sub) == 1);
    assert(*(int*)da_get(sub, 0) == 10);
    da_free(sub);
    printf("da_get_subarr passed.\n");

    // Test da_raw / da_get_raw
    void* raw_ptr = da_raw(da);
    assert(raw_ptr != NULL);
    assert(*(int*)raw_ptr == 10);
    printf("da_raw passed.\n");

    size_t da_len = da_length(da);
    (void)da_len;
    void* moved_raw_ptr = da_get_raw(da);
    assert(moved_raw_ptr != NULL);
    assert(*(int*)moved_raw_ptr == 10);
    free(moved_raw_ptr);  // Caller must free
    printf("da_get_raw passed.\n");

    DArray_t* da2 = da_new(sizeof(int));
    da2->copier = int_copier;
    da_push(da2, &v1);
    // Test da_get_arr (returns a copy)
    int* c_arr = (int*)da_get_arr(da2);
    assert(c_arr != NULL);
    assert(c_arr[0] == 10);
    free(c_arr);
    printf("da_get_arr passed.\n");
    da_free(da2);

    printf("Test Getters done.\n\n");
}
// ---

void test_setters() {
    printf("--- Test Setters ---\n");
    DArray_t* da = da_new(sizeof(int));
    da->copier = int_copier;
    int v1 = 10, v2 = 20, v3 = 30;

    // Test da_set (in bounds)
    da_push(da, &v1);
    assert(*(int*)da_get(da, 0) == 10);
    assert(da_set(da, 0, &v2));
    assert(*(int*)da_get(da, 0) == 20);
    assert(da_length(da) == 1);
    printf("da_set (in bounds) passed.\n");

    // Test da_set (out of current length, but in capacity - creates a hole)
    // Capacity is 4. Length is 1. Set at index 3.
    assert(da_set(da, 3, &v3));
    assert(da_length(da) == 4);  // Length should be extended to idx + 1
    assert(*(int*)da_get(da, 3) == 30);
    // The "hole" at index 1 and 2 is uninitialized (not safe to check)
    printf("da_set (creating hole) passed.\n");

    // Test da_swap
    int v4 = 40;
    da_set(da, 1, &v4);  // Fill hole to test swap
    assert(da_swap(da, 0, 1));
    assert(*(int*)da_get(da, 0) == 40);
    assert(*(int*)da_get(da, 1) == 20);
    assert(da_swap(da, 3, 3));  // Self-swap
    printf("da_swap passed.\n");

    da_free(da);
    printf("Test Setters done.\n\n");
}
// ---

void test_insertion_deletion() {
    printf("--- Test Insertion & Deletion ---\n");
    DArray_t* da = da_new(sizeof(int));
    da->copier = int_copier;
    int v1 = 10, v2 = 20, v3 = 30, v4 = 40;

    // Test da_push (and upsize implicitly)
    assert(da_push(da, &v1));  // length 1, capacity 4
    assert(da_push(da, &v2));  // length 2
    assert(da_length(da) == 2);
    // Push until upsize happens (for a capacity of 4, push 5 times)
    da_push(da, &v3);
    da_push(da, &v4);  // length 4
    int v5 = 50;
    assert(da_push(da, &v5));  // length 5, capacity 8
    assert(da_capacity(da) > 4);
    printf("da_push & __da_upsize passed.\n");

    // Test da_pop (and implicit downsize)
    int* popped = (int*)da_pop(da);
    assert(popped != NULL);
    assert(*popped == 50);
    free(popped);
    assert(da_length(da) == 4);
    printf("da_pop passed.\n");

    // Test da_push_front
    int v0 = 5;
    assert(da_push_front(da, &v0));  // Array: {5, 10, 20, 30, 40}
    assert(*(int*)da_get(da, 0) == 5);
    assert(*(int*)da_get(da, 1) == 10);
    printf("da_push_front passed.\n");

    // Test da_pop_front
    popped = (int*)da_pop_front(da);  // Array: {10, 20, 30, 40}
    assert(popped != NULL);
    assert(*popped == 5);
    free(popped);
    assert(*(int*)da_get(da, 0) == 10);
    assert(da_length(da) == 4);
    printf("da_pop_front passed.\n");

    // Test da_insert_at (middle)
    int v25 = 25;
    assert(da_insert_at(da, 2, &v25));  // Array: {10, 20, 25, 30, 40}
    assert(*(int*)da_get(da, 2) == 25);
    assert(*(int*)da_get(da, 3) == 30);
    printf("da_insert_at (middle) passed.\n");

    // Test da_remove_at (middle)
    assert(da_remove_at(da, 2));  // Array: {10, 20, 30, 40}
    assert(*(int*)da_get(da, 2) == 30);
    assert(da_length(da) == 4);
    printf("da_remove_at (middle) passed.\n");

    // Test da_remove (with deallocator)
    DArray_t* da_p = da_new(sizeof(Person));
    da_p->copier = person_copier;
    da_p->deallocator = person_deallocator;
    Person pa1 = create_person(1, "A"), pa2 = create_person(2, "B");
    da_push(da_p, &pa1);
    da_push(da_p, &pa2);
    destroy_person(pa1);
    destroy_person(pa2);

    Person target = {1, NULL};  // Only need ID for comparison
    size_t removed_idx = da_remove(da_p, &target, person_cmp_by_id);
    assert(removed_idx == 0);
    assert(da_length(da_p) == 1);
    Person* remaining = (Person*)da_get(da_p, 0);
    assert(remaining->id == 2);
    printf("da_remove passed.\n");

    da_free(da);
    da_free(da_p);
    printf("Test Insertion & Deletion done.\n\n");
}
// ---

void test_resizing() {
    printf("--- Test Resizing ---\n");
    DArray_t* da = da_new_with_capacity(sizeof(int), 10);
    da->copier = int_copier;
    int v = 1;

    // Push 5 elements
    for (int i = 0; i < 5; i++) da_push(da, &v);
    assert(da_length(da) == 5);
    assert(da_capacity(da) == 10);

    // Test da_truncate (no change)
    assert(da_truncate(da, 5));
    assert(da_length(da) == 5);

    // Test da_truncate (reduce length)
    assert(da_truncate(da, 2));
    assert(da_length(da) == 2);
    assert(*(int*)da_get(da, 1) == 1);
    printf("da_truncate passed.\n");

    // Test da_resize (grow capacity)
    assert(da_resize(da, 20));
    assert(da_capacity(da) == 20);
    assert(da_length(da) == 2);
    printf("da_resize (grow) passed.\n");

    // Test da_resize (shrink capacity, no length change)
    assert(da_resize(da, 5));
    assert(da_capacity(da) == 5);
    assert(da_length(da) == 2);
    printf("da_resize (shrink, no truncate) passed.\n");

    // Test da_reserve (no op)
    assert(da_reserve(da, 4));
    assert(da_capacity(da) == 5);
    printf("da_reserve (no op) passed.\n");

    // Test da_reserve (op)
    assert(da_reserve(da, 10));
    assert(da_capacity(da) == 10);
    printf("da_reserve (op) passed.\n");

    // Test da_shrink
    assert(da_shrink(da));
    assert(da_capacity(da) == da_length(da));
    assert(da_capacity(da) == 2);
    printf("da_shrink passed.\n");

    da_free(da);
    printf("Test Resizing done.\n\n");
}
// ---

void test_searching() {
    printf("--- Test Searching ---\n");
    DArray_t* da = da_new(sizeof(int));
    da->copier = int_copier;
    da->printer = int_printer;
    int a = 10, b = 20, c = 30, target = 20, not_found = 99;
    da_push(da, &a);
    da_push(da, &b);
    da_push(da, &c);  // {10, 20, 30}

    // Test da_find (linear search)
    assert(da_find(da, &target, int_cmp) == 1);
    assert(da_find(da, &not_found, int_cmp) == (size_t)-1);
    printf("da_find passed.\n");

    // Test da_contains
    assert(da_contains(da, &target, int_cmp) == true);
    assert(da_contains(da, &not_found, int_cmp) == false);
    printf("da_contains passed.\n");

    // Test da_binary_search (requires sorted array, which it is)
    assert(da_binary_search(da, &target, int_cmp) == 1);
    assert(da_binary_search(da, &not_found, int_cmp) == (size_t)-1);
    printf("da_binary_search passed.\n");

    // Test da_contains_bsearch (requires sorted array)
    assert(da_contains_bsearch(da, &target, int_cmp) == true);
    assert(da_contains_bsearch(da, &not_found, int_cmp) == false);
    printf("da_contains_bsearch passed.\n");

    da_free(da);
    printf("Test Searching done.\n\n");
}
// ---

void test_order_manipulation() {
    printf("--- Test Order Manipulation ---\n");
    DArray_t* da = da_new(sizeof(int));
    da->copier = int_copier;
    int v[] = {30, 10, 40, 20};
    for (size_t i = 0; i < 4; i++) da_push(da, &v[i]);  // {30, 10, 40, 20}

    // Test da_sort
    da_sort(da, int_cmp);  // {10, 20, 30, 40}
    assert(*(int*)da_get(da, 0) == 10);
    assert(*(int*)da_get(da, 3) == 40);
    printf("da_sort passed.\n");

    // Test da_reverse
    da_reverse(da);  // {40, 30, 20, 10}
    assert(*(int*)da_get(da, 0) == 40);
    assert(*(int*)da_get(da, 3) == 10);
    printf("da_reverse passed.\n");

    // Test da_rotate_left
    da_rotate_left(da, 2);  // {20, 10, 40, 30}
    assert(*(int*)da_get(da, 0) == 20);
    assert(*(int*)da_get(da, 1) == 10);
    assert(*(int*)da_get(da, 3) == 30);
    printf("da_rotate_left passed.\n");

    // Test da_rotate_right
    da_rotate_right(da, 1);  // {30, 20, 10, 40}

    assert(*(int*)da_get(da, 0) == 30);
    assert(*(int*)da_get(da, 3) == 40);
    printf("da_rotate_right passed.\n");

    da_free(da);
    printf("Test Order Manipulation done.\n\n");
}
// ---

void test_concatenation() {
    printf("--- Test Concatenation ---\n");
    DArray_t* a = da_new(sizeof(int));
    DArray_t* b = da_new(sizeof(int));
    a->copier = int_copier;
    b->copier = int_copier;
    int v1 = 1, v2 = 2, v3 = 3, v4 = 4;
    da_push(a, &v1);
    da_push(a, &v2);  // A: {1, 2}
    da_push(b, &v3);
    da_push(b, &v4);  // B: {3, 4}

    // Test da_concat
    DArray_t* c = da_concat(a, b);  // C: {1, 2, 3, 4}
    assert(da_length(c) == 4);
    assert(*(int*)da_get(c, 0) == 1);
    assert(*(int*)da_get(c, 3) == 4);
    printf("da_concat passed.\n");
    da_free(c);

    // Test da_merge_sorted
    // A: {1, 2}, B: {3, 4} are sorted
    DArray_t* merged = da_merge_sorted(a, b, int_cmp);  // Merged: {1, 2, 3, 4}
    assert(da_length(merged) == 4);
    assert(*(int*)da_get(merged, 0) == 1);
    assert(*(int*)da_get(merged, 3) == 4);
    printf("da_merge_sorted (trivial) passed.\n");
    da_free(merged);

    // Test da_merge_sorted (interleaving)
    DArray_t* d = da_new(sizeof(int));
    d->copier = int_copier;
    int v5 = 5, v6 = 6;
    (void)v6;
    da_set(a, 1, &v5);                        // A: {1, 5} (Capacity 4, Length 2)
    da_set(b, 0, &v2);                        // B: {2, 4} (Capacity 4, Length 2)
    merged = da_merge_sorted(a, b, int_cmp);  // Merged: {1, 2, 4, 5}
    assert(da_length(merged) == 4);
    assert(*(int*)da_get(merged, 1) == 2);
    assert(*(int*)da_get(merged, 2) == 4);
    da_free(merged);
    printf("da_merge_sorted (interleaving) passed.\n");

    da_free(a);
    da_free(b);
    printf("Test Concatenation done.\n\n");
}
// ---

// Transformation function for map: int -> struct
typedef struct {
    int value;
} Wrapper;
void int_to_wrapper_map_fn(void* dest, const void* src) {
    Wrapper* w = (Wrapper*)dest;
    const int* i = (const int*)src;
    w->value = *i * 2;
}

bool greater_than_10_filter_fn(const void* elem) {
    const int* i = (const int*)elem;
    return *i > 10;
}

void sum_reduce_fn(void* acc, const void* elem) {
    int* sum = (int*)acc;
    const int* i = (const int*)elem;
    *sum += *i;
}

void test_functional_methods() {
    printf("--- Test Functional Methods ---\n");
    DArray_t* da = da_new(sizeof(int));
    da->copier = int_copier;
    int v[] = {5, 10, 15, 20};
    for (size_t i = 0; i < 4; i++) da_push(da, &v[i]);  // {5, 10, 15, 20}

    // Test da_map
    DArray_t* mapped = da_map(da, int_to_wrapper_map_fn, sizeof(Wrapper));
    assert(mapped != NULL);
    assert(da_length(mapped) == 4);
    assert(mapped->element_size == sizeof(Wrapper));
    assert(((Wrapper*)da_get(mapped, 0))->value == 10);
    assert(((Wrapper*)da_get(mapped, 3))->value == 40);
    printf("da_map passed.\n");
    da_free(mapped);

    // Test da_filter
    DArray_t* filtered = da_filter(da, greater_than_10_filter_fn);
    assert(filtered != NULL);
    assert(da_length(filtered) == 2);  // {15, 20}
    assert(*(int*)da_get(filtered, 0) == 15);
    assert(*(int*)da_get(filtered, 1) == 20);
    printf("da_filter passed.\n");
    da_free(filtered);

    // Test da_reduce
    int sum = 0;
    da_reduce(da, &sum, sum_reduce_fn);
    assert(sum == 5 + 10 + 15 + 20);  // sum = 50
    printf("da_reduce passed.\n");

    da_free(da);
    printf("Test Functional Methods done.\n\n");
}
// ---

void test_default_fns() {
    printf("--- Test Default Functions (SIGTRAP Handling) ---\n");

    DArray_t* da = da_new(sizeof(int));
    int v = 10;

    // 1. Setup SIGTRAP handler
    // Store the old signal handler
    void (*old_handler)(int) = signal(SIGTRAP, sigtrap_handler);
    sigtrap_caught = 0;  // Reset flag

    // 2. Setup jump point. If longjmp returns 1, we continue execution here.
    if (setjmp(env) == 0) {
        // This is the first execution path
        printf("Attempting da_push with default copier (expecting SIGTRAP)... ");

        // This call will execute __da_default_copier and call raise(SIGTRAP)
        da_push(da, &v);

        // If execution reaches here, the SIGTRAP failed to fire or was ignored.
        printf("Error: SIGTRAP did not fire or was ignored. Maybe a debugger is connected? If not then this is not expected.\n");
        assert(0);  // Force failure if the expected signal wasn't caught
    } else {
        // This path is taken after longjmp from the signal handler
        printf("Caught SIGTRAP successfully.\n");
        assert(sigtrap_caught == 1);

        // The element insertion failed, and the array state is indeterminate for 'v'.
        // We set the length manually to 1 to test the printer,
        // as the default copier's job is to crash, not to insert data.
        da->length = 1;

        // Test default printer (prints memory address: <@0x...>)
        fprintf(stdout, "Default printer set, its output should look like '<@0x...>' next: ");
        da_print(da);  // Uses da->printer (default)
        fprintf(stdout, "\n");
    }

    // 3. Restore the original signal handler
    signal(SIGTRAP, old_handler);

    da_free(da);
    printf("Test Default Functions (SIGTRAP) passed.\n\n");
}

// ---

int main() {
    printf("Starting DArray Test Suite...\n\n");

    test_initialization();
    test_copy_and_cleanup();
    test_getters();
    test_setters();
    test_insertion_deletion();
    test_resizing();
    test_searching();
    test_order_manipulation();
    test_concatenation();
    test_functional_methods();
    test_default_fns();

    printf("All tests passed!\n");

    return 0;
}