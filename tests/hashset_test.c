#include "hashset.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helpers
int int_cmp(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}
uint64_t int_hasher(const void *k, uint64_t s0, uint64_t s1) {
    (void)s0;
    (void)s1;
    return (uint64_t)(*(int *)k * 2654435761);
}
void int_copier(void *dest, const void *src) {
    memcpy(dest, src, sizeof(int));
}
void int_deallocator(void *k) {
    free(k);
}
void int_printer(FILE *f, const void *k) {
    fprintf(f, "%d", *(int *)k);
}

// --- TESTS ---
void test_basic_insert() {
    HSet *hs = hs_new(sizeof(int), int_cmp, int_hasher, int_copier, int_deallocator);
    assert(hs_is_empty(hs));

    int x = 10;
    assert(hs_insert(hs, &x));
    assert(hs_count(hs) == 1);
    assert(hs_contains(hs, &x));

    int y = 20;
    hs_insert(hs, &y);
    assert(hs_count(hs) == 2);

    assert(!hs_insert(hs, &x));  // duplicate insert should fail
    assert(hs_count(hs) == 2);

    hs_free(hs);
}

void test_remove() {
    HSet *hs = hs_new(sizeof(int), int_cmp, int_hasher, int_copier, int_deallocator);
    int a = 1, b = 2;
    hs_insert(hs, &a);
    hs_insert(hs, &b);

    assert(hs_count(hs) == 2);
    assert(hs_remove(hs, &a));
    assert(!hs_contains(hs, &a));
    assert(hs_count(hs) == 1);
    assert(!hs_remove(hs, &a));  // already removed

    hs_free(hs);
}

void test_copy_and_eq() {
    HSet *hs1 = hs_new(sizeof(int), int_cmp, int_hasher, int_copier, int_deallocator);
    int a = 5, b = 6;
    hs_insert(hs1, &a);
    hs_insert(hs1, &b);

    HSet *hs2 = hs_copy(hs1);
    assert(hs_are_eq(hs1, hs2));
    assert(hs_is_subset(hs1, hs2));
    assert(hs_is_supset(hs1, hs2));

    int c = 7;
    hs_insert(hs2, &c);
    assert(!hs_are_eq(hs1, hs2));
    assert(hs_is_subset(hs1, hs2));
    assert(!hs_is_supset(hs1, hs2));

    hs_free(hs1);
    hs_free(hs2);
}

void test_set_operations() {
    int vals1[] = {1, 2, 3};
    int vals2[] = {3, 4};

    HSet *A = hs_new_from_array(sizeof(int), int_cmp, int_hasher, int_copier, int_deallocator, vals1, 3);
    HSet *B = hs_new_from_array(sizeof(int), int_cmp, int_hasher, int_copier, int_deallocator, vals2, 2);

    HSet *U = hs_union(A, B);
    assert(hs_count(U) == 4);

    HSet *I = hs_intersection(A, B);
    assert(hs_count(I) == 1 && hs_contains(I, &vals1[2]));

    HSet *D = hs_difference(A, B);
    assert(hs_count(D) == 2 && hs_contains(D, &vals1[0]));

    HSet *SD = hs_sym_difference(A, B);
    assert(hs_count(SD) == 3);

    hs_free(A);
    hs_free(B);
    hs_free(U);
    hs_free(I);
    hs_free(D);
    hs_free(SD);
}

void test_iterator() {
    int vals[] = {10, 20, 30};
    HSet *hs = hs_new_from_array(sizeof(int), int_cmp, int_hasher, int_copier, int_deallocator, vals, 3);

    HSIterator *it = hs_iterator(hs);
    int count = 0;
    while (hs_iter_next(it)) {
        int *v = hs_iter_get(it);
        assert(v != NULL);
        count++;
    }
    assert(count == 3);
    free(it);
    hs_free(hs);
}

int main() {
    test_basic_insert();
    test_remove();
    test_copy_and_eq();
    test_set_operations();
    test_iterator();

    printf("All tests passed!\n");
    return 0;
}
