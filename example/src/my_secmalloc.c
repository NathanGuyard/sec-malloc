#define _GNU_SOURCE
#include "my_secmalloc.private.h"
#include <stdio.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

// Variables globales pour les pools de mémoire
void *data_pool = NULL; // Pool de données
block_meta_t *meta_pool = NULL; // Pool de métadonnées
size_t data_pool_size = 0; // Taille du pool de données
size_t meta_pool_size = 0; // Taille du pool de métadonnées
FILE *report_file = NULL; // Fichier de rapport

// Taille initiale des pools
#define INITIAL_DATA_POOL_SIZE (1024 * 1024) // 1 Mo
#define INITIAL_META_POOL_SIZE (1024 * 1024) // 1 Mo

// Alignement des blocs de mémoire
#define ALIGNMENT 8

// Aligner la taille sur la taille de l'alignement
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

__attribute__((constructor))
void initialize_report() {
    const char *filename = getenv("MSM_OUTPUT");
    if (filename != NULL) {
        report_file = fopen(filename, "w");
        if (report_file == NULL) {
            perror("Failed to open report file");
            exit(EXIT_FAILURE);
        } else {
            fprintf(report_file, "Report file opened successfully\n");
            fflush(report_file);
        }
    }
}

__attribute__((destructor))
void close_report() {
    if (report_file != NULL) {
        fprintf(report_file, "Closing report file\n");
        fflush(report_file);
        fclose(report_file);
    }
}

// Fonction d'initialisation des pools de mémoire
static void init_pools() {
    if (report_file) {
        fprintf(report_file, "Initializing memory pools\n");
        fflush(report_file);
    }
    
    data_pool = mmap(NULL, INITIAL_DATA_POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (data_pool == MAP_FAILED) {
        if (report_file) {
            fprintf(report_file, "Error: mmap failed for data_pool\n");
            fflush(report_file);
        }
        perror("mmap data_pool");
        exit(EXIT_FAILURE);
    }
    data_pool_size = INITIAL_DATA_POOL_SIZE;

    meta_pool = mmap(NULL, INITIAL_META_POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (meta_pool == MAP_FAILED) {
        if (report_file) {
            fprintf(report_file, "Error: mmap failed for meta_pool\n");
            fflush(report_file);
        }
        perror("mmap meta_pool");
        exit(EXIT_FAILURE);
    }
    meta_pool_size = INITIAL_META_POOL_SIZE;

    meta_pool->size = data_pool_size;
    meta_pool->free = 1;
    meta_pool->next = NULL;

    if (report_file) {
        fprintf(report_file, "Memory pools initialized successfully\n");
        fflush(report_file);
    }
}

void log_memory_operation(const char *operation, size_t size, void *ptr) {
    if (report_file != NULL) {
        fprintf(report_file, "%s: size=%zu, address=%p\n", operation, size, ptr);
        fflush(report_file);
    }
}

// Fonction malloc sécurisée pour allouer de la mémoire dans le pool géré manuellement
void *my_malloc(size_t size) {
    size = ALIGN(size);

    if (!data_pool || !meta_pool) {
        init_pools();
    }

    block_meta_t *current = meta_pool;
    while (current) {
        if (current->free && current->size >= size) {
            size_t remaining_size = current->size - size - sizeof(block_meta_t);
            if (remaining_size > sizeof(block_meta_t)) {
                block_meta_t *new_meta = (block_meta_t *)((char *)current + sizeof(block_meta_t) + size);
                new_meta->size = remaining_size;
                new_meta->free = 1;
                new_meta->next = current->next;
                current->size = size;
                current->next = new_meta;
            }
            current->free = 0;
            void *allocated_space = (char *)current + sizeof(block_meta_t);
            log_memory_operation("malloc", size, allocated_space);
            return (char *)current + sizeof(block_meta_t);
        }
        current = current->next;
    }
    if (report_file) {
        fprintf(report_file, "malloc failed: size=%zu\n", size);
        fflush(report_file);
    }
    return NULL;
}

// Fonction de libération de mémoire sécurisée
void my_free(void *ptr) {
    if (!ptr) return;

    block_meta_t *meta = (block_meta_t *)((char *)ptr - sizeof(block_meta_t));
    log_memory_operation("free", 0, ptr);
    meta->free = 1;

    block_meta_t *current = meta_pool;
    while (current) {
        if (current->free && current->next && current->next->free) {
            current->size += sizeof(block_meta_t) + current->next->size;
            current->next = current->next->next;
        }
        current = current->next;
    }
}

// Fonction calloc sécurisée pour allouer et initialiser de la mémoire
void *my_calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    void *ptr = my_malloc(total_size);
    if (ptr) {
        memset(ptr, 0, total_size);
        log_memory_operation("calloc", total_size, ptr);
    }
    return ptr;
}

// Fonction realloc sécurisée pour redimensionner un bloc de mémoire alloué
void *my_realloc(void *ptr, size_t size) {
    if (!ptr) return my_malloc(size);
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    block_meta_t *meta = (block_meta_t *)((char *)ptr - sizeof(block_meta_t));
    if (meta->size >= size) {
        log_memory_operation("realloc", size, ptr);
        return ptr;
    }

    void *new_ptr = my_malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, meta->size);
        my_free(ptr);
        log_memory_operation("realloc", size, new_ptr);
    }
    return new_ptr;
}

// Fonctions pour bibliothèque dynamique
#ifdef DYNAMIC
__attribute__((visibility("default")))
void *malloc(size_t size) {
    return my_malloc(size);
}

__attribute__((visibility("default")))
void free(void *ptr) {
    my_free(ptr);
}

__attribute__((visibility("default")))
void *calloc(size_t nmemb, size_t size) {
    return my_calloc(nmemb, size);
}

__attribute__((visibility("default")))
void *realloc(void *ptr, size_t size) {
    return my_realloc(ptr, size);
}
#endif
