#define _GNU_SOURCE
#include "my_secmalloc.private.h"
#include <stdio.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

/* Directives de préprocesseur pour l'alignement sur 8 octets / 4 octets selon l'architecture */
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(__ppc64__)
    #define ALIGNMENT 8
#elif defined(__i386__) || defined(_M_IX86) || defined(__arm__) || defined(__PPC__)
    #define ALIGNMENT 4
#else
    #define ALIGNMENT 8
#endif

#define MY_PAGE_SIZE (size_t)4096
#define INITIAL_MMAP_SIZE (MY_PAGE_SIZE * 1000) // 4 Mo
#define ALIGN(size) (size_t)((size + (ALIGNMENT - 1)) & (~(ALIGNMENT - 1)))
#define ALIGN_DOWN(x) ((x) & ~(ALIGNMENT-1))

metadata *meta_pool = NULL;
topchunk *topchunk_pool = NULL;
void *data_pool = NULL;

static void init_pools(void) {
    /* Construction du top_chunk */

    /* On commence la liste des metadata 40 Mo après l'adresse nulle en croisant les doigts */
    topchunk_pool = mmap((size_t*)(MY_PAGE_SIZE * 10000), INITIAL_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(topchunk_pool == MAP_FAILED)
    {
        perror("mmap meta_pool");
        exit(1);
    }

    /* TODO : topchunk_pool canary */
    topchunk_pool->canary = (size_t)0xdeadbeefcafebabe;
    topchunk_pool->total_size_metadata = INITIAL_MMAP_SIZE;
    topchunk_pool->current_size_metadata = 0;
    topchunk_pool->number_of_elements_allocated = 0;
    topchunk_pool->number_of_elements_freed = 0;
    topchunk_pool->free_metadata = NULL;

    /* On commence la liste des data 40 Go après l'adresse nulle en croisant les doigts */
    data_pool = mmap((size_t*)(MY_PAGE_SIZE * 10000000), INITIAL_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(data_pool == MAP_FAILED)
    {
        perror("mmap meta_pool");
        exit(1);
    }

    topchunk_pool->current_size_data = 0;
    topchunk_pool->total_size_data = INITIAL_MMAP_SIZE;

    meta_pool = (metadata*)((size_t)topchunk_pool + ALIGN(sizeof(topchunk)));

    /* Espace d'environ 4 Go qui sépare la liste des metadata à la data pool */
}

size_t *verify_freed_block(size_t size)
{
    if(topchunk_pool->number_of_elements_freed == 0) return NULL;
    if(meta_pool != NULL)
    {
        metadata *current_meta = meta_pool;
        void* current_chunk; 
        while(current_meta != NULL)
        {
            current_chunk = current_meta->chunk;
            if(current_meta->free == MY_IS_FREE && current_meta->size_of_chunk + ALIGN(sizeof(void*)) >= size + ALIGN(sizeof(void*)))
            {
                /* On a trouvé un bloc qui a été free et qui a une taille convenable pour allouer à nouveau */
                if(current_meta->size_of_chunk + ALIGN(sizeof(void*)) > size + ALIGN(sizeof(void*)))
                {
                    /* Le bloc trouvé a une taille supérieure à la taille demandée : fragmentation du bloc */
                    metadata *new_one;
                    size_t remaining_size = current_meta->size_of_chunk - (size + ALIGN(sizeof(void*)));
                    if(current_meta->next == NULL)
                    {
                        /* On a trouvé le dernier bloc de la liste */
                        if(remaining_size >= size + ALIGN(sizeof(size_t)))
                        {
                            /* Il y a de la place pour un autre bloc */
                            if(topchunk_pool->free_metadata == NULL)
                            {
                                /* Liste des metadata non utilisés vide */
                                new_one = (metadata*)((size_t)current_meta + ALIGN(sizeof(current_meta)));
                            }
                            else
                            {
                                new_one = topchunk_pool->free_metadata;
                                if(new_one->next == NULL)
                                {
                                    topchunk_pool->free_metadata = NULL;
                                }
                                else
                                {
                                    topchunk_pool->free_metadata = new_one->next;
                                }
                            }
                            new_one->size_of_chunk = remaining_size;
                            /* TODO : canary */
                            new_one->canary_chunk = (size_t)0xdeadbeefcafebabe;
                            new_one->chunk = (void*)((size_t)current_chunk + size + ALIGN(sizeof(size_t)));
                            new_one->free = MY_IS_FREE;
                            new_one->next = NULL;
                            current_meta->next = new_one;
                            current_meta->size_of_chunk = size;
                            topchunk_pool->number_of_elements_allocated++;
                        }
                        else
                        {
                            /* Il n'y a pas assez de place pour un autre bloc 
                            Le chunk retourné prends la place restante dans sa data, qui ne sera pas alignée */
                            current_meta->next = NULL;
                            topchunk_pool->number_of_elements_allocated++;
                            topchunk_pool->number_of_elements_freed--;
                        }
                    }
                    else
                    {
                        /* On a trouvé un bloc au début / milieu / vers la fin de la liste */
                        metadata *next_one = current_meta->next;
                        next_one->free = MY_IS_FREE;
                        if(remaining_size >= size + ALIGN(sizeof(size_t)))
                        {
                            /* Il y a de la place pour un autre bloc */
                            if(topchunk_pool->free_metadata == NULL)
                            {
                                /* Liste des metadata non utilisés vide */
                                new_one = (metadata*)((size_t)current_meta + ALIGN(sizeof(current_meta)));
                            }
                            else
                            {
                                new_one = topchunk_pool->free_metadata;
                                if(new_one->next == NULL)
                                {
                                    topchunk_pool->free_metadata = NULL;
                                }
                                else
                                {
                                    topchunk_pool->free_metadata = new_one->next;
                                }
                            }
                            current_meta->next = new_one;
                            new_one->next = next_one;
                            new_one->size_of_chunk = remaining_size;
                            /* TODO : canary */
                            new_one->canary_chunk = (size_t)0xdeadbeefcafebabe;
                            new_one->chunk = (void*)((size_t)current_chunk + size + ALIGN(sizeof(size_t)));
                            new_one->free = MY_IS_FREE;
                            current_meta->size_of_chunk = size;
                            topchunk_pool->number_of_elements_allocated++;
                        }
                        else
                        {
                            /* Il n'y a pas assez de place pour un autre bloc
                            Le chunk retourné prends la place restante dans sa data, qui ne sera pas alignée */
                            topchunk_pool->number_of_elements_allocated++;
                            topchunk_pool->number_of_elements_freed--;
                        }
                    }
                    current_meta->free = MY_IS_BUSY;
                    /* TODO : canary */
                    size_t *canary = (size_t*)((size_t)current_chunk + current_meta->size_of_chunk);
                    *canary = (size_t)0xdeadbeefcafebabe;
                    return (size_t*)(current_meta->chunk);
                }
                else
                {
                    /* Le bloc trouvé a la taille parfaite : pas de fragmentation de bloc */
                    topchunk_pool->number_of_elements_allocated++;
                    topchunk_pool->number_of_elements_freed--;
                    current_meta->free = MY_IS_BUSY;
                    /* TODO : canary */
                    size_t *canary = (size_t*)((size_t)current_chunk + current_meta->size_of_chunk);
                    *canary = (size_t)0xdeadbeefcafebabe;
                    return (size_t*)(current_meta->chunk);
                }
            }
            current_meta = current_meta->next;
        }
    }
    return NULL;
}

void get_more_memory_mmap_metadata(void)
{
    /* Augmenter de la taille d'une page */
    void *ptr = mremap(topchunk_pool,topchunk_pool->total_size_metadata, topchunk_pool->total_size_metadata + MY_PAGE_SIZE, MREMAP_MAYMOVE);
    if(topchunk_pool == MAP_FAILED || ptr != topchunk_pool)
    {
        perror("mmap meta_pool");
        exit(1);
    }
    topchunk_pool->total_size_metadata = topchunk_pool->total_size_metadata + MY_PAGE_SIZE;
}

void get_more_memory_mmap_data(size_t size)
{
    /* Augmenter de size + ALIGN(sizeof(size_t)) pour le canary (aligné sur une page !) */
    size_t new_aligned_size = (size_t)(((topchunk_pool->total_size_data + size + ALIGN(sizeof(size_t))) + (MY_PAGE_SIZE - 1)) & (~(MY_PAGE_SIZE - 1)));
    void *ptr = mremap(data_pool,topchunk_pool->total_size_data, new_aligned_size, MREMAP_MAYMOVE);
    if(topchunk_pool == MAP_FAILED || ptr != data_pool)
    {
        perror("mmap meta_pool");
        exit(1);
    }
    topchunk_pool->total_size_data = new_aligned_size;
}

void *my_malloc(size_t size) {
    /* Permet d'aligner la taille sur 8 ou 4 octets, et permet d'allouer au minimum 8 ou 4 octets pour une taille nulle */
    size = (size == 0) ? ALIGN(1) : ALIGN(size);

    /* Si le top_chunk n'a jamais été crée, alors le créer.
     Cela signifie que c'est le tout premier malloc du programme */
    if (topchunk_pool == NULL) {
        init_pools();
    }
    
    size_t *freed_block = verify_freed_block(size);
    if(freed_block != NULL)
    {
        return freed_block;
    }
    
    if(topchunk_pool->current_size_metadata + ALIGN(sizeof(metadata)) > topchunk_pool->total_size_metadata)
    {
        /* on demande + de mémoire pour meta_pool avec mremap */
        get_more_memory_mmap_metadata();
    }

    if(topchunk_pool->current_size_data + ALIGN(sizeof(size_t)) + size > topchunk_pool->total_size_data)
    {
        /* on demande + de mémoire pour data_pool avec mremap */
        get_more_memory_mmap_data(size);
    }

    metadata *new_meta = (metadata*)((size_t)meta_pool + topchunk_pool->current_size_metadata);
    /* TODO : canary */
    new_meta->canary_chunk = (size_t)0xdeadbeefcafebabe;
    new_meta->canary = (size_t)0xdeadbeefcafebabe;
    new_meta->chunk = data_pool + topchunk_pool->current_size_data;
    new_meta->free = MY_IS_BUSY;
    new_meta->next = NULL;
    new_meta->size_of_chunk = size; /* data sans le canary */
    topchunk_pool->current_size_data += size + ALIGN(sizeof(size_t));
    
    if(topchunk_pool->number_of_elements_allocated != 0 || topchunk_pool->number_of_elements_allocated != 0)
    {
        /* Pas la première fois qu'on alloue quelque chose */
        metadata *last_one = (metadata*)((size_t)meta_pool + topchunk_pool->current_size_metadata - ALIGN(sizeof(metadata)));
        last_one->next = new_meta;
    }

    topchunk_pool->current_size_metadata += ALIGN(sizeof(metadata));
    topchunk_pool->number_of_elements_allocated++;
    // TODO : canary
    size_t *canary = (size_t*)((size_t)new_meta->chunk + new_meta->size_of_chunk);
    *canary = (size_t)0xdeadbeefcafebabe;

    return new_meta->chunk;
}

unsigned char find_element_to_free(void *ptr)
{
    metadata *current_meta = meta_pool;
    void *current_chunk;
    while(current_meta != NULL)
    {
        current_chunk = current_meta->chunk;
        if(current_chunk == ptr)
        {
            if(current_meta->free == MY_IS_BUSY)
            {
                /* Le bloc devient libre */
                current_meta->free = MY_IS_FREE;
                /* Ajouter à la taille du bloc free le canary de fin */
                current_meta->size_of_chunk += ALIGN(sizeof(size_t));
                topchunk_pool->number_of_elements_allocated--;
                topchunk_pool->number_of_elements_freed++;
                if(topchunk_pool->free_metadata == NULL)
                {
                    /* Si tous les metadata sont utilisés, initier la liste */
                    topchunk_pool->free_metadata = current_meta;
                    current_meta->next = NULL;
                }
                else
                {
                    /* Ajouter en début de liste */
                    metadata *first = topchunk_pool->free_metadata;
                    topchunk_pool->free_metadata = current_meta;
                    current_meta->next = first;
                }
                current_meta->free = MY_IS_NOT_HERE;
                return 1;
            }
            else
            {
                /* Double free found ! */
                return 2;
            }
        }
        current_meta = current_meta->next;
    }
    /* Not found */
    return 0;
}

void merge_freed_blocks(void)
{
    metadata *current_meta = meta_pool;
    void *current_chunk;
    while(current_meta != NULL)
    {
        current_chunk = current_meta->next;
        if(current_meta->free == MY_IS_FREE && current_meta->next != NULL && current_meta->next->free == MY_IS_FREE && (metadata*)((size_t)current_chunk + current_meta->size_of_chunk) == current_meta->next)
        {
            /* Si deux blocs sont contigus dans la mémoire ET libres */
            /* On peut merge en toute sécurité */
            current_meta->size_of_chunk += current_meta->next->size_of_chunk;
            topchunk_pool->number_of_elements_freed--;
            
            // if(current_meta->next->next != NULL)
            // {
            //     /* Si en milieu ou début de liste */
            //     current_meta->size_of_chunk += current_meta->next->size_of_chunk;
            //     /* Deux blocs en un, donc on enlève un bloc */
            //     /* current_meta->next = current_meta->next->next; */
            //     topchunk_pool->number_of_elements_freed--;
            // }
            // else
            // {
            //     /* Dernier élément de la liste */
            //     current_meta->size_of_chunk += current_meta->next->size_of_chunk;
            //     current_meta->next = NULL;
            //     topchunk_pool->number_of_elements_freed--;
            // }

            /* On repart du bloc fusionné */
            continue;
        }
        current_meta = current_meta->next;
    }
}

void my_free(void *ptr) {
    /* Pas de free(NULL) possible */
    if(ptr == NULL)
    {
        /* On tente de free NULL */
        return;
    }
    if(topchunk_pool == NULL)
    {
        /* On tente de free alors que rien n'a été alloué */
        return;
    }

    unsigned char found = find_element_to_free(ptr);
    switch(found)
    {
        case 0:
            /* not found */
            break;
        case 1:
            /* found */
            break;
        case 2:
            /* double free ! */
            printf("DOUBLE FREE FOUND !\n");
            exit(1);
            break;
    }
    merge_freed_blocks();
}

// Fonction calloc sécurisée pour allouer et initialiser de la mémoire
void *my_calloc(size_t nmemb, size_t size) {
    // TODO
    return (size_t*)0xdeadbeef;
}


// Fonction realloc sécurisée pour redimensionner un bloc de mémoire alloué
void *my_realloc(void *ptr, size_t size) {
    // TODO
    return (size_t*)0xcafebabe;
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