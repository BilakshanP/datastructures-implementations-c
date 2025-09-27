#include <signal.h>
#include <stdlib.h>

#include "../lib/darray.h"

void printer(const void* k) {
    if (!k) raise(SIGTRAP);

    printf("%d", *(int*)k);
}
void copier(void* d, const void* s) { *(int*)d = *(int*)s; }
int cmp(const void* a, const void* b) { return *(int*)a - *(int*)b; }

bool even_filter(const void* k) { return (*(int*)k) % 2 == 0; }

int main() {
    DArray* da = da_new(sizeof(int));

    da->growth_factor = 10;

    da->copier = copier;
    da->printer = printer;

    for (int i = 0; i < 10; i++) {
        int* n = (int*)malloc(da->element_size);
        *n = i;

        if (!da_push_front(da, n)) raise(SIGTRAP);
    }

    printf("Initial array: ");
    da_print(da);
    printf("\n");

    DArray* filtered = da_filter(da, even_filter);

    da_reverse(filtered);
    da_shrink(filtered);

    da_print(filtered);
    printf("%ld %ld \n", filtered->length, filtered->capacity);
}
