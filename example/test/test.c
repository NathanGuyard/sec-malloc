#include <criterion/criterion.h>  // Inclut la bibliothèque Criterion pour les tests unitaires
#include "my_secmalloc.private.h"  // Inclut les déclarations des fonctions et structures internes de l'allocateur
#include <sys/mman.h>  // Inclut les définitions de mmap et munmap

// Test simple de mmap et munmap pour vérifier l'allocation et la libération de mémoire
Test(mmap, simple) {
    // Allouer une page de mémoire (4096 octets) avec mmap
    void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    // Vérifier que l'allocation a réussi (ptr ne doit pas être NULL)
    cr_expect(ptr != NULL);
    // Libérer la mémoire allouée avec munmap
    int res = munmap(ptr, 4096);
    // Vérifier que la libération a réussi (res doit être 0)
    cr_expect(res == 0);
}

// Test de base pour my_malloc pour vérifier l'allocation et la libération de mémoire
Test(my_malloc, basic_allocation) {
    // Allouer 100 octets de mémoire avec my_malloc
    void *ptr = my_malloc(100);
    // Vérifier que l'allocation a réussi (ptr ne doit pas être NULL)
    cr_assert_not_null(ptr, "Allocation should succeed");
    // Libérer la mémoire allouée avec my_free
    my_free(ptr);
}

// Test de multiples allocations pour vérifier la gestion des blocs multiples
Test(my_malloc, multiple_allocations) {
    // Allouer 100 octets de mémoire avec my_malloc
    void *ptr1 = my_malloc(100);
    // Allouer 200 octets de mémoire avec my_malloc
    void *ptr2 = my_malloc(200);
    // Vérifier que la première allocation a réussi (ptr1 ne doit pas être NULL)
    cr_assert_not_null(ptr1, "First allocation should succeed");
    // Vérifier que la deuxième allocation a réussi (ptr2 ne doit pas être NULL)
    cr_assert_not_null(ptr2, "Second allocation should succeed");
    // Libérer la première mémoire allouée avec my_free
    my_free(ptr1);
    // Libérer la deuxième mémoire allouée avec my_free
    my_free(ptr2);
}

// Test de l'allocation avec my_calloc pour vérifier l'initialisation à zéro
Test(my_malloc, calloc_allocation) {
    // Allouer de la mémoire pour un tableau de 10 éléments de 10 octets chacun avec my_calloc
    void *ptr = my_calloc(10, 10);
    // Vérifier que l'allocation a réussi (ptr ne doit pas être NULL)
    cr_assert_not_null(ptr, "Calloc should succeed");
    // Vérifier que tous les octets alloués sont initialisés à zéro
    for (size_t i = 0; i < 100; i++) {
        cr_assert(((char *)ptr)[i] == 0, "Calloc should initialize memory to zero");
    }
    // Libérer la mémoire allouée avec my_free
    my_free(ptr);
}

// Test de la réallocation avec my_realloc pour vérifier le redimensionnement du bloc de mémoire
Test(my_malloc, realloc_allocation) {
    // Allouer 100 octets de mémoire avec my_malloc
    void *ptr = my_malloc(100);
    // Vérifier que l'allocation a réussi (ptr ne doit pas être NULL)
    cr_assert_not_null(ptr, "Allocation should succeed");

    // Réallouer le bloc de mémoire à une nouvelle taille de 200 octets avec my_realloc
    void *new_ptr = my_realloc(ptr, 200);
    // Vérifier que la réallocation a réussi (new_ptr ne doit pas être NULL)
    cr_assert_not_null(new_ptr, "Reallocation should succeed");
    // Libérer la mémoire réallouée avec my_free
    my_free(new_ptr);
}