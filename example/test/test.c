#include <criterion/criterion.h>
#include "my_secmalloc.private.h"  // Inclut les déclarations des fonctions d'allocateur
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>

/* Directives de préprocesseur pour l'alignement sur 8 octets / 4 octets selon l'architecture */
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(__ppc64__)
    #define ALIGNMENT 8
#elif defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(__PPC__)
    #define ALIGNMENT 4
#else
    #define ALIGNMENT 8
#endif

// Test simple de mmap et munmap pour vérifier l'allocation et la libération de mémoire
Test(mmap, simple) {
    char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    cr_expect(ptr != MAP_FAILED, "mmap should not fail");
    int res = munmap(ptr, 4096);
    cr_expect(res == 0, "munmap should succeed");
}

// Test de base pour my_malloc pour vérifier l'allocation et la libération de mémoire
Test(my_malloc, basic_allocation) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Allocation should succeed");
    my_free(ptr);
}

// Test de multiples allocations pour vérifier la gestion des blocs multiples
Test(my_malloc, multiple_allocations) {
    char *ptr1 = my_malloc(100);
    char *ptr2 = my_malloc(200);
    cr_assert_not_null(ptr1, "First allocation should succeed");
    cr_assert_not_null(ptr2, "Second allocation should succeed");
    my_free(ptr1);
    my_free(ptr2);
}

// Test de l'allocation avec my_calloc pour vérifier l'initialisation à zéro
Test(my_malloc, calloc_allocation) {
    char *ptr = my_calloc(10, 10);
    cr_assert_not_null(ptr, "Calloc should succeed");
    for (size_t i = 0; i < 100; i++) {
        cr_assert(((char *)ptr)[i] == 0, "Calloc should initialize memory to zero");
    }
    my_free(ptr);
}

// Test de la réallocation avec my_realloc pour vérifier le redimensionnement du bloc de mémoire
Test(my_malloc, realloc_allocation) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Allocation should succeed");
    strcpy(ptr, "test");
    cr_assert_str_eq(ptr, "test", "String should be 'test'");
    
    char *new_ptr = my_realloc(ptr, 200);
    cr_assert_not_null(new_ptr, "Reallocation should succeed");
    cr_assert_str_eq(new_ptr, "test", "String should still be 'test' after realloc");
    strcat(new_ptr, " realloc");
    cr_assert_str_eq(new_ptr, "test realloc", "String should be 'test realloc'");
    my_free(new_ptr);
}

// Test de stress pour vérifier la stabilité de l'allocateur sous forte charge
Test(my_malloc, stress_test) {
    const int N = 1000;
    char **ptrs = my_malloc(N * sizeof(char*));
    for (int i = 0; i < N; i++) {
        size_t size = (i % 10 + 1) * 10;
        ptrs[i] = my_malloc(size);
        memset(ptrs[i], 0, size);
        cr_assert_not_null(ptrs[i], "Allocation failed at iteration %d", i);
    }
    for (int i = 0; i < N; i++) {
        my_free(ptrs[i]);
    }
    my_free(ptrs);
}

// Test pour vérifier l'allocation et la libération avec des tailles variées
Test(my_malloc, varied_allocations) {
    char *ptr1 = my_malloc(10);
    char *ptr2 = my_malloc(50);
    char *ptr3 = my_malloc(100);
    char *ptr4 = my_malloc(200);

    cr_assert_not_null(ptr1, "Allocation of 10 bytes should succeed");
    cr_assert_not_null(ptr2, "Allocation of 50 bytes should succeed");
    cr_assert_not_null(ptr3, "Allocation of 100 bytes should succeed");
    cr_assert_not_null(ptr4, "Allocation of 200 bytes should succeed");

    my_free(ptr1);
    my_free(ptr2);
    my_free(ptr3);
    my_free(ptr4);
}

// Test pour vérifier l'allocation et la libération avec des tailles de zéro et petites tailles
Test(my_malloc, zero_and_small_allocations) {
    char *ptr1 = my_malloc(0);
    char *ptr2 = my_malloc(1);
    char *ptr3 = my_malloc(2);

    cr_assert_not_null(ptr1, "Allocation of 0 bytes should succeed (returns non-NULL)");
    cr_assert_not_null(ptr2, "Allocation of 1 byte should succeed");
    cr_assert_not_null(ptr3, "Allocation of 2 bytes should succeed");

    my_free(ptr1);
    my_free(ptr2);
    my_free(ptr3);
}

// Test pour vérifier le comportement de my_free avec un pointeur NULL
Test(my_malloc, free_null) {
    my_free(NULL);
    cr_assert(true, "Freeing NULL should not crash");
}

// Test pour vérifier le comportement de my_free avec un double free
Test(my_malloc, double_free) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Allocation should succeed");

    my_free(ptr);
    cr_assert(true, "First free should succeed");

    // Cette opération devrait provoquer une erreur dans le log ou une sortie
    // Commenter ou décommenter la ligne suivante selon la gestion de double free souhaitée
    //my_free(ptr);
    // Utilisez cr_assert_fail si vous attendez une défaillance ici
}

// Test pour vérifier la fragmentation et la réutilisation des blocs libres
Test(my_malloc, fragmentation_and_reuse) {
    char *ptr1 = my_malloc(100);
    char *ptr2 = my_malloc(200);
    char *ptr3 = my_malloc(150);

    cr_assert_not_null(ptr1, "First allocation should succeed");
    cr_assert_not_null(ptr2, "Second allocation should succeed");
    cr_assert_not_null(ptr3, "Third allocation should succeed");

    my_free(ptr2);

    char *ptr4 = my_malloc(50);
    cr_assert_not_null(ptr4, "Fourth allocation should succeed and reuse the freed block");

    my_free(ptr1);
    my_free(ptr3);
    my_free(ptr4);
}

// Test pour vérifier le comportement de my_malloc avec une allocation énorme
Test(my_malloc, large_allocation) {
    char *ptr = my_malloc(10 * 1024 * 1024); // 10 Mo
    cr_assert_not_null(ptr, "Large allocation should succeed");
    my_free(ptr);
}

// Test pour vérifier l'allocation et la libération dans une boucle
Test(my_malloc, loop_allocation_free) {
    for (int i = 0; i < 1000; i++) {
        char *ptr = my_malloc(100);
        cr_assert_not_null(ptr, "Allocation should succeed in loop iteration %d", i);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et l'initialisation avec my_calloc dans une boucle
Test(my_malloc, loop_calloc) {
    for (int i = 0; i < 1000; i++) {
        char *ptr = my_calloc(10, 10);
        cr_assert_not_null(ptr, "Calloc should succeed in loop iteration %d", i);
        for (size_t j = 0; j < 100; j++) {
            cr_assert(ptr[j] == 0, "Calloc should initialize memory to zero in iteration %d", i);
        }
        my_free(ptr);
    }
}

// Test pour vérifier la réallocation avec my_realloc dans une boucle
Test(my_malloc, loop_realloc) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Initial allocation should succeed");
    for (int i = 0; i < 100; i++) {
        char *new_ptr = my_realloc(ptr, 100 + i * 10);
        cr_assert_not_null(new_ptr, "Reallocation should succeed in loop iteration %d", i);
        ptr = new_ptr;
    }
    my_free(ptr);
}

// Test pour vérifier l'initialisation avec my_calloc et la réutilisation des blocs de mémoire
Test(my_malloc, calloc_and_reuse) {
    char *ptr1 = my_calloc(10, 10);
    cr_assert_not_null(ptr1, "Calloc should succeed");

    for (size_t i = 0; i < 100; i++) {
        cr_assert(ptr1[i] == 0, "Calloc should initialize memory to zero");
    }

    my_free(ptr1);

    char *ptr2 = my_malloc(50);
    cr_assert_not_null(ptr2, "Allocation after calloc should succeed");
    my_free(ptr2);
}

// Test pour vérifier la libération de blocs de tailles différentes
Test(my_malloc, free_varied_sizes) {
    char *ptr1 = my_malloc(10);
    char *ptr2 = my_malloc(50);
    char *ptr3 = my_malloc(100);

    cr_assert_not_null(ptr1, "Allocation of 10 bytes should succeed");
    cr_assert_not_null(ptr2, "Allocation of 50 bytes should succeed");
    cr_assert_not_null(ptr3, "Allocation of 100 bytes should succeed");

    my_free(ptr2);
    my_free(ptr1);
    my_free(ptr3);
}

// Test pour vérifier la réallocation vers une taille plus petite
Test(my_malloc, realloc_smaller_size) {
    char *ptr = my_malloc(200);
    cr_assert_not_null(ptr, "Initial allocation should succeed");
    strcpy(ptr, "test");
    cr_assert_str_eq(ptr, "test", "String should be 'test'");

    char *new_ptr = my_realloc(ptr, 100);
    cr_assert_not_null(new_ptr, "Reallocation should succeed");
    cr_assert_str_eq(new_ptr, "test", "String should still be 'test' after realloc to smaller size");

    my_free(new_ptr);
}

// Test pour vérifier la réallocation vers une taille plus grande avec initialisation des nouveaux octets à zéro
Test(my_malloc, realloc_larger_size_initialized) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Initial allocation should succeed");
    memset(ptr, 'a', 100);
    cr_assert_str_eq(ptr, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "Memory should be initialized to 'a'");

    char *new_ptr = my_realloc(ptr, 200);
    cr_assert_not_null(new_ptr, "Reallocation should succeed");
    cr_assert_str_eq(new_ptr, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "String should still be 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' after realloc to larger size");

    for (size_t i = 100; i < 200; i++) {
        cr_assert(new_ptr[i] == 0, "Newly allocated memory should be initialized to zero");
    }

    my_free(new_ptr);
}

// Test pour vérifier l'allocation et la libération avec des tailles alignées
Test(my_malloc, aligned_allocations) {
    char *ptr1 = my_malloc(8);
    char *ptr2 = my_malloc(16);
    char *ptr3 = my_malloc(32);
    char *ptr4 = my_malloc(64);

    cr_assert_not_null(ptr1, "Allocation of 8 bytes should succeed");
    cr_assert((size_t)ptr1 % 8 == 0, "Allocation should be aligned to 8 bytes");

    cr_assert_not_null(ptr2, "Allocation of 16 bytes should succeed");
    cr_assert((size_t)ptr2 % 8 == 0, "Allocation should be aligned to 8 bytes");

    cr_assert_not_null(ptr3, "Allocation of 32 bytes should succeed");
    cr_assert((size_t)ptr3 % 8 == 0, "Allocation should be aligned to 8 bytes");

    cr_assert_not_null(ptr4, "Allocation of 64 bytes should succeed");
    cr_assert((size_t)ptr4 % 8 == 0, "Allocation should be aligned to 8 bytes");

    my_free(ptr1);
    my_free(ptr2);
    my_free(ptr3);
    my_free(ptr4);
}

// Test pour vérifier la libération de mémoire non allouée
Test(my_malloc, free_unallocated_memory) {
    char dummy;
    my_free(&dummy);
    cr_assert(true, "Freeing unallocated memory should not crash");
}

// Test pour vérifier l'allocation d'une chaîne de caractères et la modification du contenu
Test(my_malloc, string_allocation_modification) {
    char *str = my_malloc(50);
    cr_assert_not_null(str, "Allocation should succeed");

    strcpy(str, "Hello, world!");
    cr_assert_str_eq(str, "Hello, world!", "String should be 'Hello, world!'");

    strcpy(str, "New string");
    cr_assert_str_eq(str, "New string", "String should be 'New string'");

    my_free(str);
}

// Test pour vérifier l'allocation et la libération dans des conditions de faible mémoire
Test(my_malloc, low_memory_allocation) {
    size_t size = 0xffffffffffffffff; // Taille maximale pour provoquer une allocation échouée
    char *ptr = my_malloc(size-1);
    cr_assert_null(ptr, "Allocation of huge size should fail");
}

// Test pour vérifier l'allocation avec des tailles croissantes
Test(my_malloc, increasing_sizes) {
    for (size_t i = 1; i <= 1024; i *= 2) {
        char *ptr = my_malloc(i);
        cr_assert_not_null(ptr, "Allocation of %zu bytes should succeed", i);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation avec des tailles décroissantes
Test(my_malloc, decreasing_sizes) {
    for (size_t i = 1024; i >= 1; i /= 2) {
        char *ptr = my_malloc(i);
        cr_assert_not_null(ptr, "Allocation of %zu bytes should succeed", i);
        my_free(ptr);
    }
}

// Test pour vérifier la réallocation avec rétrécissement du bloc
Test(my_malloc, realloc_shrink) {
    char *ptr = my_malloc(200);
    cr_assert_not_null(ptr, "Initial allocation should succeed");
    strcpy(ptr, "Shrink test");

    char *new_ptr = my_realloc(ptr, 100);
    cr_assert_not_null(new_ptr, "Reallocation to smaller size should succeed");
    cr_assert_str_eq(new_ptr, "Shrink test", "String should still be 'Shrink test' after realloc");

    my_free(new_ptr);
}

// Test pour vérifier l'allocation de multiples blocs et leur libération
Test(my_malloc, multiple_blocks_free) {
    char *ptr1 = my_malloc(100);
    char *ptr2 = my_malloc(100);
    char *ptr3 = my_malloc(100);

    cr_assert_not_null(ptr1, "First allocation should succeed");
    cr_assert_not_null(ptr2, "Second allocation should succeed");
    cr_assert_not_null(ptr3, "Third allocation should succeed");

    my_free(ptr2);
    my_free(ptr1);
    my_free(ptr3);
}

// Test pour vérifier l'allocation et la libération rapide
Test(my_malloc, rapid_allocation_free) {
    for (int i = 0; i < 1000; i++) {
        char *ptr = my_malloc(10);
        cr_assert_not_null(ptr, "Allocation should succeed in iteration %d", i);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation de mémoire partagée avec un autre processus (pas directement supporté par my_malloc, mais pour illustration)
Test(my_malloc, shared_memory_allocation) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Allocation should succeed");
    strcpy(ptr, "Shared memory test");

    // Ici, nous ne pouvons pas réellement partager la mémoire avec un autre processus,
    // mais nous simulons l'idée d'une mémoire partagée
    cr_assert_str_eq(ptr, "Shared memory test", "String should be 'Shared memory test'");
    my_free(ptr);
}

// Test pour vérifier l'allocation avec memset pour chaque bloc alloué
Test(my_malloc, memset_allocation) {
    for (int i = 0; i < 100; i++) {
        char *ptr = my_malloc(50);
        cr_assert_not_null(ptr, "Allocation should succeed");
        memset(ptr, i, 50);
        for (int j = 0; j < 50; j++) {
            cr_assert(ptr[j] == i, "Memory should be set to %d", i);
        }
        my_free(ptr);
    }
}

// Test pour vérifier la libération de mémoire dans l'ordre inverse de l'allocation
Test(my_malloc, reverse_free_order) {
    char *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = my_malloc(100);
        cr_assert_not_null(ptrs[i], "Allocation should succeed for iteration %d", i);
    }
    for (int i = 9; i >= 0; i--) {
        my_free(ptrs[i]);
    }
}

// Test pour vérifier l'allocation et l'utilisation d'un tableau de structures
typedef struct {
    int a;
    double b;
    char c;
} my_struct;

Test(my_malloc, struct_allocation) {
    my_struct *arr = my_malloc(10 * sizeof(my_struct));
    cr_assert_not_null(arr, "Allocation of struct array should succeed");

    for (int i = 0; i < 10; i++) {
        arr[i].a = i;
        arr[i].b = i * 2.0;
        arr[i].c = 'a' + i;
    }

    for (int i = 0; i < 10; i++) {
        cr_assert(arr[i].a == i, "Struct element a should be %d", i);
        cr_assert(arr[i].b == i * 2.0, "Struct element b should be %f", i * 2.0);
        cr_assert(arr[i].c == 'a' + i, "Struct element c should be %c", 'a' + i);
    }

    my_free(arr);
}

// Test pour vérifier l'allocation et la libération de mémoire alignée
Test(my_malloc, aligned_memory_allocation) {
    for (size_t align = 0; align <= 64; align++) {
        char *ptr = my_malloc(align);
        cr_assert_not_null(ptr, "Allocation should succeed for alignment %zu", align);
        cr_assert((size_t)ptr % ALIGNMENT == 0, "Memory should be aligned to %zu bytes", align);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération de mémoire avec des tailles aléatoires
#include <stdlib.h>
#include <time.h>

Test(my_malloc, random_sizes_allocation) {
    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        size_t size = rand() % 1000 + 1;
        char *ptr = my_malloc(size);
        cr_assert_not_null(ptr, "Allocation should succeed for size %zu", size);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération avec my_calloc de tailles variées
Test(my_malloc, varied_calloc) {
    for (size_t nmemb = 1; nmemb <= 10; nmemb++) {
        for (size_t size = 1; size <= 10; size++) {
            char *ptr = my_calloc(nmemb, size);
            cr_assert_not_null(ptr, "Calloc should succeed for nmemb %zu and size %zu", nmemb, size);
            for (size_t i = 0; i < nmemb * size; i++) {
                cr_assert(ptr[i] == 0, "Calloc should initialize memory to zero");
            }
            my_free(ptr);
        }
    }
}

// Test pour vérifier la réallocation de blocs à des tailles multiples de l'initiale
Test(my_malloc, realloc_multiple_sizes) {
    char *ptr = my_malloc(50);
    cr_assert_not_null(ptr, "Initial allocation should succeed");
    strcpy(ptr, "Realloc multiple sizes");

    for (int i = 1; i <= 5; i++) {
        char *new_ptr = my_realloc(ptr, 50 * i);
        cr_assert_not_null(new_ptr, "Reallocation to %d times initial size should succeed", i);
        cr_assert_str_eq(new_ptr, "Realloc multiple sizes", "String should still be 'Realloc multiple sizes' after realloc");
        ptr = new_ptr;
    }

    my_free(ptr);
}

// Test pour vérifier l'allocation et la libération en utilisant my_malloc et my_realloc en alternance
Test(my_malloc, malloc_realloc_alternate) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Initial allocation should succeed");

    for (int i = 1; i <= 10; i++) {
        ptr = my_realloc(ptr, 100 * i);
        cr_assert_not_null(ptr, "Reallocation should succeed for iteration %d", i);
        memset(ptr, i, 100 * i);
    }

    my_free(ptr);
}

// Test pour vérifier l'allocation et la libération avec des chaînes de caractères dans différentes langues
Test(my_malloc, multilingual_strings) {
    const char *strings[] = {
        "Hello", // English
        "Bonjour", // French
        "Hola", // Spanish
        "こんにちは", // Japanese
        "안녕하세요", // Korean
        "你好", // Chinese
        "Здравствуйте", // Russian
        "Hallo", // German
        "Ciao", // Italian
        "مرحبا" // Arabic
    };

    for (int i = 0; i < 10; i++) {
        char *ptr = my_malloc(strlen(strings[i]) + 1);
        cr_assert_not_null(ptr, "Allocation should succeed for string %d", i);
        strcpy(ptr, strings[i]);
        cr_assert_str_eq(ptr, strings[i], "String should be correctly copied");
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération de mémoire avec des structures imbriquées
typedef struct {
    int id;
    char name[50];
    struct {
        int day;
        int month;
        int year;
    } birthdate;
} person;

Test(my_malloc, nested_struct_allocation) {
    person *p = my_malloc(sizeof(person));
    cr_assert_not_null(p, "Allocation should succeed");

    p->id = 1;
    strcpy(p->name, "John Doe");
    p->birthdate.day = 15;
    p->birthdate.month = 6;
    p->birthdate.year = 1990;

    cr_assert(p->id == 1, "ID should be 1");
    cr_assert_str_eq(p->name, "John Doe", "Name should be 'John Doe'");
    cr_assert(p->birthdate.day == 15, "Day should be 15");
    cr_assert(p->birthdate.month == 6, "Month should be 6");
    cr_assert(p->birthdate.year == 1990, "Year should be 1990");

    my_free(p);
}

// Test pour vérifier l'allocation et la libération de mémoire avec des tailles proches des limites de page
Test(my_malloc, near_page_limit_allocation) {
    size_t page_size = 4096;
    for (size_t size = page_size - 100; size <= page_size + 100; size += 50) {
        char *ptr = my_malloc(size);
        cr_assert_not_null(ptr, "Allocation should succeed for size %zu", size);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération avec my_calloc de tailles très grandes
Test(my_malloc, large_calloc) {
    size_t nmemb = 1024;
    size_t size = 1024;
    char *ptr = my_calloc(nmemb, size);
    cr_assert_not_null(ptr, "Calloc should succeed for large allocation");
    for (size_t i = 0; i < nmemb * size; i++) {
        cr_assert(ptr[i] == 0, "Calloc should initialize memory to zero");
    }
    my_free(ptr);
}

// Test pour vérifier le comportement de my_malloc et my_free avec des tailles multiples de 4096 (taille de page)
Test(my_malloc, page_size_multiples) {
    for (size_t i = 1; i <= 10; i++) {
        char *ptr = my_malloc(4096 * i);
        cr_assert_not_null(ptr, "Allocation of %zu pages should succeed", i);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération avec des tailles de puissance de deux
Test(my_malloc, power_of_two_sizes) {
    for (size_t i = 1; i <= 1024; i *= 2) {
        char *ptr = my_malloc(i);
        cr_assert_not_null(ptr, "Allocation of %zu bytes should succeed", i);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération avec des tailles décroissantes
Test(my_malloc, decremental_sizes) {
    for (size_t i = 1024; i >= 1; i /= 2) {
        char *ptr = my_malloc(i);
        cr_assert_not_null(ptr, "Allocation of %zu bytes should succeed", i);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération de plusieurs blocs de tailles variées
Test(my_malloc, multiple_varied_allocations) {
    size_t sizes[] = {128, 256, 512, 1024, 2048};
    char *ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = my_malloc(sizes[i]);
        cr_assert_not_null(ptrs[i], "Allocation of %zu bytes should succeed", sizes[i]);
    }
    for (int i = 0; i < 5; i++) {
        my_free(ptrs[i]);
    }
}

// Test pour vérifier l'allocation et la libération en utilisant un modèle de remplissage
Test(my_malloc, pattern_allocation) {
    char pattern[] = "ABCDEF";
    for (int i = 0; i < 10; i++) {
        char *ptr = my_malloc(50);
        cr_assert_not_null(ptr, "Allocation should succeed");
        memset(ptr, pattern[i % 6], 50);
        for (int j = 0; j < 50; j++) {
            cr_assert(ptr[j] == pattern[i % 6], "Memory should be set to pattern %c", pattern[i % 6]);
        }
        my_free(ptr);
    }
}

// Test pour vérifier la libération multiple et la réutilisation des blocs de mémoire
Test(my_malloc, multiple_free_reuse) {
    char *ptr1 = my_malloc(100);
    char *ptr2 = my_malloc(200);
    my_free(ptr1);
    char *ptr3 = my_malloc(50);
    my_free(ptr2);
    my_free(ptr3);
    char *ptr4 = my_malloc(300);
    cr_assert_not_null(ptr4, "Allocation should succeed after multiple frees");
    my_free(ptr4);
}

// Test pour vérifier la libération et l'allocation avec des blocs imbriqués
Test(my_malloc, nested_allocations) {
    char *ptr1 = my_malloc(100);
    cr_assert_not_null(ptr1, "First allocation should succeed");
    char *ptr2 = my_malloc(200);
    cr_assert_not_null(ptr2, "Second allocation should succeed");

    my_free(ptr1);
    my_free(ptr2);
    char *ptr3 = my_malloc(150);
    char *ptr4 = my_malloc(250);
    cr_assert_not_null(ptr3, "Third allocation should succeed");
    cr_assert_not_null(ptr4, "Fourth allocation should succeed");
    my_free(ptr3);
    my_free(ptr4);
}

// Test pour vérifier l'allocation avec des tailles aléatoires et memset
Test(my_malloc, random_sizes_memset) {
    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        size_t size = rand() % 500 + 1;
        char *ptr = my_malloc(size);
        cr_assert_not_null(ptr, "Allocation should succeed for size %zu", size);
        memset(ptr, i, size);
        for (size_t j = 0; j < size; j++) {
            cr_assert(ptr[j] == i, "Memory should be set to %d", i);
        }
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération avec des structures contenant des pointeurs
typedef struct node {
    int value;
    struct node *next;
} node;

Test(my_malloc, linked_list_allocation) {
    node *head = my_malloc(sizeof(node));
    cr_assert_not_null(head, "Allocation for head node should succeed");
    head->value = 1;
    head->next = my_malloc(sizeof(node));
    cr_assert_not_null(head->next, "Allocation for second node should succeed");
    head->next->value = 2;
    head->next->next = NULL;

    cr_assert(head->value == 1, "Head node value should be 1");
    cr_assert(head->next->value == 2, "Second node value should be 2");

    my_free(head->next);
    my_free(head);
}

// Test pour vérifier l'allocation et la libération de mémoire avec une structure contenant des tableaux
typedef struct {
    int values[10];
    char name[50];
} complex_struct;

Test(my_malloc, complex_struct_allocation) {
    complex_struct *cs = my_malloc(sizeof(complex_struct));
    cr_assert_not_null(cs, "Allocation for complex struct should succeed");

    for (int i = 0; i < 10; i++) {
        cs->values[i] = i * 10;
    }
    strcpy(cs->name, "Complex Struct");

    for (int i = 0; i < 10; i++) {
        cr_assert(cs->values[i] == i * 10, "Values array should contain correct values");
    }
    cr_assert_str_eq(cs->name, "Complex Struct", "Name should be 'Complex Struct'");

    my_free(cs);
}

// Test pour vérifier la réallocation avec réduction de taille multiple fois
Test(my_malloc, multiple_realloc_shrink) {
    char *ptr = my_malloc(500);
    cr_assert_not_null(ptr, "Initial allocation should succeed");
    strcpy(ptr, "Realloc shrink test");

    for (int i = 4; i >= 1; i--) {
        char *new_ptr = my_realloc(ptr, 100 * i);
        cr_assert_not_null(new_ptr, "Reallocation to %d times smaller size should succeed", i);
        cr_assert_str_eq(new_ptr, "Realloc shrink test", "String should still be 'Realloc shrink test' after realloc");
        ptr = new_ptr;
    }

    my_free(ptr);
}

// Test pour vérifier la libération et réallocation rapide
Test(my_malloc, rapid_free_realloc) {
    char *ptr = my_malloc(100);
    cr_assert_not_null(ptr, "Initial allocation should succeed");

    for (int i = 0; i < 100; i++) {
        my_free(ptr);
        ptr = my_malloc(100);
        cr_assert_not_null(ptr, "Reallocation should succeed in iteration %d", i);
    }

    my_free(ptr);
}

// Test pour vérifier la libération et l'allocation avec memset et tailles variées
Test(my_malloc, varied_memset) {
    for (int i = 0; i < 100; i++) {
        size_t size = (i % 10 + 1) * 10;
        char *ptr = my_malloc(size);
        cr_assert_not_null(ptr, "Allocation should succeed for size %zu", size);
        memset(ptr, i, size);
        for (size_t j = 0; j < size; j++) {
            cr_assert(ptr[j] == i, "Memory should be set to %d", i);
        }
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération avec des tailles très petites
Test(my_malloc, very_small_allocations) {
    for (size_t i = 1; i <= 10; i++) {
        char *ptr = my_malloc(i);
        cr_assert_not_null(ptr, "Allocation of %zu bytes should succeed", i);
        my_free(ptr);
    }
}

// Test pour vérifier l'allocation et la libération avec des tailles très grandes
Test(my_malloc, very_large_allocations) {
    for (size_t i = 10 * 1024 * 1024; i <= 50 * 1024 * 1024; i += 10 * 1024 * 1024) {
        char *ptr = my_malloc(i);
        cr_assert_not_null(ptr, "Allocation of %zu bytes should succeed", i);
        my_free(ptr);
    }
}

// Test pour vérifier la fragmentation extrême
Test(my_malloc, extreme_fragmentation) {
    char *ptrs[1000];
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = my_malloc(10);
        cr_assert_not_null(ptrs[i], "Allocation should succeed");
    }
    for (int i = 0; i < 1000; i += 2) {
        my_free(ptrs[i]);
    }
    for (int i = 1; i < 1000; i += 2) {
        my_free(ptrs[i]);
    }
}

// Test pour vérifier une séquence complexe d'allocations, de réallocations et de libérations
Test(my_malloc, complex_sequence) {
    // Étape 1: Allocations initiales
    char *ptr1 = my_malloc(100);
    char *ptr2 = my_malloc(200);
    char *ptr3 = my_malloc(300);
    
    cr_assert_not_null(ptr1, "First allocation should succeed");
    cr_assert_not_null(ptr2, "Second allocation should succeed");
    cr_assert_not_null(ptr3, "Third allocation should succeed");

    // Étape 2: Initialisation et vérification
    strcpy(ptr1, "First block");
    strcpy(ptr2, "Second block");
    strcpy(ptr3, "Third block");

    cr_assert_str_eq(ptr1, "First block", "String in first block should be 'First block'");
    cr_assert_str_eq(ptr2, "Second block", "String in second block should be 'Second block'");
    cr_assert_str_eq(ptr3, "Third block", "String in third block should be 'Third block'");

    // Étape 3: Réallocation de ptr2 à une taille plus grande
    char *ptr2_new = my_realloc(ptr2, 400);
    cr_assert_not_null(ptr2_new, "Reallocation of second block should succeed");
    cr_assert_str_eq(ptr2_new, "Second block", "String in reallocated second block should be 'Second block'");

    // Étape 4: Réallocation de ptr3 à une taille plus petite
    char *ptr3_new = my_realloc(ptr3, 150);
    cr_assert_not_null(ptr3_new, "Reallocation of third block to smaller size should succeed");
    cr_assert_str_eq(ptr3_new, "Third block", "String in reallocated third block should be 'Third block'");

    // Étape 5: Libération des blocs
    my_free(ptr1);
    my_free(ptr2_new);
    my_free(ptr3_new);

    // Étape 6: Nouvelle série d'allocations pour vérifier la réutilisation de la mémoire
    char *ptr4 = my_malloc(50);
    char *ptr5 = my_malloc(250);
    char *ptr6 = my_malloc(350);

    cr_assert_not_null(ptr4, "Fourth allocation should succeed");
    cr_assert_not_null(ptr5, "Fifth allocation should succeed");
    cr_assert_not_null(ptr6, "Sixth allocation should succeed");

    // Libération finale
    my_free(ptr4);
    my_free(ptr5);
    my_free(ptr6);
}