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
    topchunk_pool->last_metadata_allocated = NULL;

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
    void * chunk_to_free = NULL;
    metadata *meta_linked_to_chunk_to_free = NULL;
    metadata *_ = meta_pool;
    for(size_t i=0;i<(topchunk_pool->number_of_elements_allocated + topchunk_pool->number_of_elements_freed);i++)
    {
        metadata *m = (metadata*)((size_t)_ + (i * ALIGN(sizeof(metadata))));
        if(m->size_of_chunk + ALIGN(sizeof(size_t)) >= size + ALIGN(sizeof(size_t)) && m->free == MY_IS_FREE)
        {
            chunk_to_free = m->chunk;
            meta_linked_to_chunk_to_free = m;
            break;
        }
    }
    if(meta_pool != NULL)
    {
        metadata *current_meta = topchunk_pool->free_metadata;
        while(current_meta != NULL)
        {
            /* Si un bloc libre a une taille suffisante
                    Si la taille est supérieure alors on fragmente
                        Si présence de free_metadata alors on utilise jusqu'à épuisement sinon on crée des meta à la fin
                    Si la taille est parfaite
                        Si présence de free_metadata alors on utilise jusqu'à épuisement sinon on crée des meta à la fin
                Sinon on retourne NULL
            */

           /*
            On a la liste des meta qui sont seuls
            Il faut la liste des free et raccrocher la liste des meta a la liste des free
           */

           if(chunk_to_free != NULL)
           {
                /* On a trouvé un data qui a été free et qui a une taille convenable pour allouer à nouveau */
                void* current_chunk = chunk_to_free;
                /* Si un chunk est partagé par deux meta qui ont été free, intervertir les pointeurs de chunk */
                if(current_meta != meta_linked_to_chunk_to_free)
                {
                    void *tmp_chunk = current_meta->chunk;
                    current_meta->chunk = meta_linked_to_chunk_to_free->chunk;
                    meta_linked_to_chunk_to_free->chunk = tmp_chunk;
                }
                if(current_meta->size_of_chunk > size + ALIGN(sizeof(size_t)))
                {
                    /* Le bloc trouvé a une taille supérieure à la taille demandée : fragmentation du bloc */
                    /* Priorité aux meta qui attendent */
                    metadata *new_frag;
                    size_t size_second_block_contiguous = 0;
                    if(topchunk_pool->free_metadata != NULL)
                    {
                        new_frag = topchunk_pool->free_metadata;
                        if(new_frag->next_waiting == NULL)
                        {
                            topchunk_pool->free_metadata = NULL;
                        }
                        else
                        {
                            topchunk_pool->free_metadata = new_frag->next_waiting;
                        }
                    }
                    else
                    {
                        /* new_frag_next sera contigu dans la mémoire car toutes les meta sont occuppées */
                        new_frag = (metadata*)((size_t)current_meta + ALIGN(sizeof(current_meta)));
                        size_second_block_contiguous = ALIGN(sizeof(current_meta));
                    }
                    size_t remaining_size = current_meta->size_of_chunk - (size + ALIGN(sizeof(size_t)));
                    metadata *new_frag_next;
                    if(new_frag->next_waiting != NULL)
                    {
                        /* Si un meta est au moins free */
                        new_frag_next = new_frag->next_waiting;
                        if(new_frag_next->next_waiting == NULL)
                        {
                            topchunk_pool->free_metadata = NULL;
                        }
                        else
                        {
                            topchunk_pool->free_metadata = new_frag_next->next_waiting;
                        }
                    }
                    else
                    {
                        /* Si aucun meta n'est free, alors new_frag_next se colle à new_frag */
                        new_frag_next = (metadata*)((size_t)current_meta + ALIGN(sizeof(metadata)) + size_second_block_contiguous);
                    }

                    // TODO : canary
                    size_t *canary = (size_t*)((size_t)current_chunk + size);
                    *canary = (size_t)0xdeadbeefcafebabe;
                    new_frag->canary = (size_t)0xdeadbeefcafebabe;
                    new_frag->canary_chunk = (size_t)0xdeadbeefcafebabe;
                    new_frag->free = MY_IS_BUSY;
                    new_frag->size_of_chunk = size;
                    new_frag->next_waiting = NULL;
                    new_frag->chunk = current_chunk;
                    if(topchunk_pool->last_metadata_allocated != NULL)
                    {
                        topchunk_pool->last_metadata_allocated->next = new_frag;
                    }
                    topchunk_pool->last_metadata_allocated = new_frag;

                    // TODO : canary
                    new_frag_next->canary = (size_t)0xdeadbeefcafebabe;
                    new_frag_next->canary_chunk = (size_t)0xdeadbeefcafebabe;
                    new_frag_next->free = MY_IS_FREE;
                    new_frag_next->size_of_chunk = remaining_size;
                    new_frag_next->chunk = (void*)((size_t)current_chunk + size + ALIGN(sizeof(size_t)));
                    if(size_second_block_contiguous != 0)
                    {
                        /* Signifie que new_frag_next est contigu */
                        new_frag->next = new_frag_next;
                        new_frag_next->next = NULL;
                    }
                    else
                    {
                        if(topchunk_pool->free_metadata != NULL)
                        {
                            topchunk_pool->free_metadata->next_waiting = new_frag_next;
                        }
                        else
                        {
                            topchunk_pool->free_metadata = new_frag_next;
                        }
                        new_frag_next->next_waiting = NULL;
                        topchunk_pool->last_metadata_allocated->next = new_frag_next; // bug ?
                    }
                    topchunk_pool->number_of_elements_allocated++;
                }
                else
                {
                    /* Le bloc trouvé a la taille parfaite : pas de fragmentation de bloc */
                    /* Priorité aux meta qui attendent */
                    metadata *new_frag;
                    if(topchunk_pool->free_metadata != NULL)
                    {
                        new_frag = topchunk_pool->free_metadata;
                        if(new_frag->next_waiting == NULL)
                        {
                            topchunk_pool->free_metadata = NULL;
                        }
                        else
                        {
                            topchunk_pool->free_metadata = new_frag->next_waiting;
                        }
                    }
                    else
                    {
                        new_frag = (metadata*)((size_t)current_meta + ALIGN(sizeof(current_meta)));
                    }

                    // TODO : canary
                    size_t *canary = (size_t*)((size_t)current_chunk + size);
                    *canary = (size_t)0xdeadbeefcafebabe;
                    new_frag->canary = (size_t)0xdeadbeefcafebabe;
                    new_frag->canary_chunk = (size_t)0xdeadbeefcafebabe;
                    new_frag->free = MY_IS_BUSY;
                    new_frag->size_of_chunk = size;
                    new_frag->next_waiting = NULL;
                    new_frag->chunk = current_chunk;
                    if(topchunk_pool->last_metadata_allocated != NULL)
                    {
                        topchunk_pool->last_metadata_allocated->next = new_frag;
                    }
                    topchunk_pool->last_metadata_allocated = new_frag;
                    topchunk_pool->number_of_elements_allocated++;
                    topchunk_pool->number_of_elements_freed--;
                }
                
                return current_chunk;
           }
           current_meta = current_meta->next_waiting;
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

    if(topchunk_pool->current_size_metadata + 2 * ALIGN(sizeof(metadata)) > topchunk_pool->total_size_metadata)
    {
        /* on demande + de mémoire pour meta_pool avec mremap dans le cas extrême de 2 allocs pour fragmentation de data */
        get_more_memory_mmap_metadata();
    }

    if(topchunk_pool->current_size_data + ALIGN(sizeof(size_t)) + size > topchunk_pool->total_size_data)
    {
        /* on demande + de mémoire pour data_pool avec mremap */
        get_more_memory_mmap_data(size);
    }

    size_t *freed_block = verify_freed_block(size);
    if(freed_block != NULL)
    {
        return freed_block;
    }

    /* Ajout du metadata à la fin de la liste */
    metadata *new_meta = (metadata*)((size_t)meta_pool + topchunk_pool->current_size_metadata);
    /* TODO : canary */
    new_meta->canary_chunk = (size_t)0xdeadbeefcafebabe;
    new_meta->canary = (size_t)0xdeadbeefcafebabe;
    new_meta->chunk = data_pool + topchunk_pool->current_size_data;
    new_meta->free = MY_IS_BUSY;
    new_meta->next = NULL;
    new_meta->next_waiting = NULL;
    new_meta->size_of_chunk = size; /* data sans le canary */
    topchunk_pool->current_size_data += size + ALIGN(sizeof(size_t));

    // if(topchunk_pool->number_of_elements_allocated != 0 || topchunk_pool->number_of_elements_freed != 0)
    // {
    //     /* Pas la première fois qu'on alloue ou free quelque chose */
    //     metadata *previous = (metadata*)((size_t)meta_pool + topchunk_pool->current_size_metadata - ALIGN(sizeof(metadata)));
    //     previous->next = new_meta;
    // }

    if(topchunk_pool->last_metadata_allocated != NULL)
    {
        if(topchunk_pool->last_metadata_allocated->next != NULL)
        {
            metadata *previous = topchunk_pool->last_metadata_allocated->next;
            new_meta->next = previous;
            topchunk_pool->last_metadata_allocated = new_meta;
        }
        else
        {
            metadata *z = topchunk_pool->last_metadata_allocated;
            new_meta->next = z;
            topchunk_pool->last_metadata_allocated = new_meta;
            topchunk_pool->last_metadata_allocated->next = NULL;
        }
    }
    else
    {
        topchunk_pool->last_metadata_allocated = new_meta;
    }
    if(topchunk_pool->free_metadata != NULL)
    {
        topchunk_pool->free_metadata = topchunk_pool->free_metadata->next_waiting;
    }
    else
    {
        topchunk_pool->free_metadata = NULL;
    }

    topchunk_pool->current_size_metadata += ALIGN(sizeof(metadata));
    topchunk_pool->number_of_elements_allocated++;
    // TODO : canary
    size_t *canary = (size_t*)((size_t)new_meta->chunk + new_meta->size_of_chunk);
    *canary = (size_t)0xdeadbeefcafebabe;
    /*metadata *o = topchunk_pool->last_metadata_allocated;
    for(size_t i=0;i<topchunk_pool->number_of_elements_allocated-1;i++)
    {
        o = o->next;
    }
    o->next = NULL;*/

    return new_meta->chunk;
}

unsigned char find_element_to_free(void *ptr)
{
    metadata *previous_meta = NULL;
    metadata *previous_meta_allocated = NULL;
    metadata *current_meta = NULL;
    void *current_chunk;

    for(size_t i=0;i<(topchunk_pool->number_of_elements_allocated + topchunk_pool->number_of_elements_freed);i++)
    {
        current_meta = (metadata*)((size_t)meta_pool + i * ALIGN(sizeof(metadata))); 
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
                    topchunk_pool->free_metadata->next_waiting = NULL;
                }
                else
                {
                    /* Ajouter en début de liste */
                    metadata *first = topchunk_pool->free_metadata;
                    topchunk_pool->free_metadata = current_meta;
                    topchunk_pool->free_metadata->next_waiting = first;
                }
                if(current_meta == topchunk_pool->last_metadata_allocated)
                {
                    /* Si ce bloc est le dernier élément de la liste des éléments alloués */
                    topchunk_pool->last_metadata_allocated = previous_meta_allocated;
                }

                return 1;
            }
            else
            {
                /* Double free found ! */
                return 2;
            }
        }
        previous_meta = current_meta;
        if(previous_meta->free == MY_IS_BUSY)
        {
            previous_meta_allocated = previous_meta;
        }
    }
    /* Not found */
    return 0;
}

// void merge_freed_blocks(void)
// {
//     metadata *current_meta = meta_pool;
//     void *current_chunk;
//     while(current_meta != NULL)
//     {
//         current_chunk = current_meta->next;
//         if(current_meta->free == MY_IS_FREE && current_meta->next != NULL && current_meta->next->free == MY_IS_FREE && (metadata*)((size_t)current_chunk + current_meta->size_of_chunk) == current_meta->next)
//         {
//             /* Si deux blocs sont contigus dans la mémoire ET libres */
//             /* On peut merge en toute sécurité */
//             current_meta->size_of_chunk += current_meta->next->size_of_chunk;
//             topchunk_pool->number_of_elements_freed--;

//             // if(current_meta->next->next != NULL)
//             // {
//             //     /* Si en milieu ou début de liste */
//             //     current_meta->size_of_chunk += current_meta->next->size_of_chunk;
//             //     /* Deux blocs en un, donc on enlève un bloc */
//             //     /* current_meta->next = current_meta->next->next; */
//             //     topchunk_pool->number_of_elements_freed--;
//             // }
//             // else
//             // {
//             //     /* Dernier élément de la liste */
//             //     current_meta->size_of_chunk += current_meta->next->size_of_chunk;
//             //     current_meta->next = NULL;
//             //     topchunk_pool->number_of_elements_freed--;
//             // }

//             /* On repart du bloc fusionné */
//             continue;
//         }
//         current_meta = current_meta->next;
//     }
// }

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
    //merge_freed_blocks();
}

void *my_calloc(size_t nmemb, size_t size) {
    char *ptr = NULL;
    if(nmemb == 0 || size == 0)
    {
        ptr = my_malloc(ALIGNMENT);
        for(size_t i = 0;i<ALIGNMENT;i++)
        {
            ptr[i] = 0;
        }
        return ptr;
    }

    ptr = my_malloc(nmemb * size);
    for(size_t i = 0;i<(nmemb * size);i++)
    {
        ptr[i] = 0;
    }
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    if(ptr != NULL && size == 0)
    {
        my_free(ptr);
        return NULL;
    }

    /* Libérer le chunk et le reloger */
    my_free(ptr);

    if(ptr == NULL) return my_malloc(size);
    if(size == 0) return my_malloc(0);

    return my_malloc(size);
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