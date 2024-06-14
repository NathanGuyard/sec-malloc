#define _GNU_SOURCE
#include "my_secmalloc.private.h"
#include <stdio.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

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
FILE *report_file = NULL;

void logfile(const char *format, ...) {
    if(report_file == NULL) return;

    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (size < 0) {
        perror("Error vsnprintf");
        va_end(args);
        return;
    }

    char *message = (char *)alloca(size + 1);
    if (message == NULL) {
        perror("Error alloca");
        va_end(args);
        return;
    }

    vsnprintf(message, size + 1, format, args);

    size_t written = fwrite(message, 1, size, report_file);
    if (written < (size_t)size) {
        perror("Error fwrite");
    }
    // if (fprintf(report_file, "%s", message) < 0) {
    //     perror("Error fprintf");
    // }

    va_end(args);
    fflush(report_file);
}

__attribute__((constructor))
void initialize_report() {
    const char *filename = getenv("MSM_OUTPUT");
    if (filename != NULL) {
        report_file = fopen(filename, "w");
        if (report_file == NULL) {
            perror("Failed to open report file");
            exit(EXIT_FAILURE);
        } else {
            fflush(report_file);
        }
    }
}

__attribute__((destructor))
void close_report() {
    if (report_file != NULL) {
        fclose(report_file);
    }
}

static void init_pools(void) {
    /* Construction du top_chunk */

    /* On commence la liste des metadata 40 Mo après l'adresse nulle en croisant les doigts */
    topchunk_pool = mmap((size_t*)(MY_PAGE_SIZE * 10000), INITIAL_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(topchunk_pool == MAP_FAILED)
    {
        logfile("*** ERROR *** : mmap topchunk_pool failed.\nExit !\n");
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
    topchunk_pool->metadata_allocated = NULL;

    /* On commence la liste des data 40 Go après l'adresse nulle en croisant les doigts */
    data_pool = mmap((size_t*)(MY_PAGE_SIZE * 10000000), INITIAL_MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(data_pool == MAP_FAILED)
    {
        logfile("*** ERROR *** : mmap data_pool failed.\nExit !\n");
        perror("mmap meta_pool");
        exit(1);
    }

    topchunk_pool->current_size_data = 0;
    topchunk_pool->total_size_data = INITIAL_MMAP_SIZE;

    meta_pool = (metadata*)((size_t)topchunk_pool + ALIGN(sizeof(topchunk)));

    logfile("==============================[ Start Pools Initialisation ]==============================\n");
    logfile("[+] topchunk_pool mapped @ %p\n",topchunk_pool);
    logfile("[+] meta_pool mapped @ %p\n",meta_pool);
    logfile("==============================[ End Pools Initialisation ]==============================\n\n");

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
        /* Pour chaque bloc indépendemment de leur ordre, on récupère le meta qui est free et qui a une taille de data suffisante */
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
                    if(topchunk_pool->free_metadata != NULL)
                    {
                        /* Si au moins un meta est free alors on le libère pour en faire un occuppé */
                        new_frag = topchunk_pool->free_metadata;
                        metadata *_ = topchunk_pool->free_metadata;
                        metadata *previous = NULL;
                        while(_ != NULL)
                        {
                            if(_->next_waiting == new_frag)   
                            {
                                previous = _;
                                break;
                            }
                            _ = _->next_waiting;
                        }
                        /* Si le bloc avait un suivant alors raccrocher son précédent avec son suivant pour l'enlever de la liste des éléments alloués */
                        if(previous != NULL)
                        {
                            /* Le bloc avait un meta free précédent */
                            if(new_frag->next_waiting != NULL)
                            {
                                /* Le bloc avait un meta free suivant et précédent */
                                metadata *next_one = new_frag->next_waiting;
                                previous->next_waiting = next_one;
                            }
                            else
                            {
                                /* Le bloc n'avait qu'un meta free précédent */
                                previous->next_waiting = NULL;
                            }
                        }
                        else
                        {
                            /* Le bloc n'avait pas de meta free précédent */
                            if(new_frag->next_waiting != NULL)
                            {
                                /* Le bloc n'avait qu'un meta free suivant */
                                metadata *next_one = new_frag->next_waiting;
                                topchunk_pool->free_metadata = next_one;
                                next_one->next_waiting = NULL;
                            }
                            else
                            {
                                /* Le bloc n'avait ni de meta free suivant ni précédent */
                                topchunk_pool->free_metadata = NULL;
                            }
                        }
                    }
                    else
                    {
                        /* new_frag_next sera contigu dans la mémoire car toutes les meta sont occuppées */
                        new_frag = (metadata*)((size_t)meta_pool + ALIGN(sizeof(metadata)) * (topchunk_pool->number_of_elements_allocated + topchunk_pool->number_of_elements_freed));
                        topchunk_pool->current_size_metadata += ALIGN(sizeof(metadata));
                    }
                    // TODO : canary
                    size_t *canary = (size_t*)((size_t)current_chunk + size);
                    *canary = (size_t)0xaabbccddddccbbaa;
                    new_frag->canary = (size_t)0xdeadbeefcafebabe;
                    new_frag->canary_chunk = (size_t)0xdeadbeefcafebabe;
                    new_frag->free = MY_IS_BUSY;
                    new_frag->next_waiting = NULL;
                    new_frag->chunk = current_chunk;
                    new_frag->next = NULL;
                    /* Si au moins un élément est déjà alloué, mettre le nouveau meta alloué à la suite */
                    if(topchunk_pool->metadata_allocated != NULL)
                    {
                        metadata *n = topchunk_pool->metadata_allocated;
                        while(n->next != NULL){n = n->next;}
                        n->next = new_frag;
                    }
                    else
                    {
                        topchunk_pool->metadata_allocated = new_frag;
                    }
                    size_t remaining_size = new_frag->size_of_chunk - (size + ALIGN(sizeof(size_t)));
                    new_frag->size_of_chunk = size;
                    metadata *new_frag_next;
                    if(topchunk_pool->free_metadata != NULL)
                    {
                        /* Si au moins un meta est free alors on le libère pour en faire un occuppé */
                        new_frag_next = topchunk_pool->free_metadata;
                        metadata *_ = topchunk_pool->free_metadata;
                        metadata *previous = NULL;
                        while(_ != NULL)
                        {
                            if(_->next_waiting == new_frag_next)   
                            {
                                previous = _;
                                break;
                            }
                            _ = _->next_waiting;
                        }
                        /* Si le bloc avait un suivant alors raccrocher son précédent avec son suivant pour l'enlever de la liste des éléments alloués */
                        if(previous != NULL)
                        {
                            /* Le bloc avait un meta free précédent */
                            if(new_frag_next->next_waiting != NULL)
                            {
                                /* Le bloc avait un meta free suivant et précédent */
                                metadata *next_one = new_frag_next->next_waiting;
                                previous->next_waiting = next_one;
                            }
                            else
                            {
                                /* Le bloc n'avait qu'un meta free précédent */
                                previous->next_waiting = NULL;
                            }
                        }
                        else
                        {
                            /* Le bloc n'avait pas de meta free précédent */
                            if(new_frag_next->next_waiting != NULL)
                            {
                                /* Le bloc n'avait qu'un meta free suivant */
                                metadata *next_one = new_frag_next->next_waiting;
                                topchunk_pool->free_metadata = next_one;
                                next_one->next_waiting = NULL;
                            }
                            else
                            {
                                /* Le bloc n'avait ni de meta free suivant ni précédent */
                                topchunk_pool->free_metadata = NULL;
                            }
                        }
                    }
                    else
                    {
                        /* Si aucun meta n'est free, alors new_frag_next se colle à new_frag */
                        new_frag_next = (metadata*)((size_t)meta_pool + topchunk_pool->current_size_metadata);
                        topchunk_pool->current_size_metadata += ALIGN(sizeof(metadata));
                    }

                    // TODO : canary
                    new_frag_next->canary = (size_t)0xdeadbeefcafebabe;
                    new_frag_next->canary_chunk = (size_t)0xdeadbeefcafebabe;
                    new_frag_next->free = MY_IS_FREE;
                    new_frag_next->size_of_chunk = remaining_size;
                    new_frag_next->next = NULL;
                    new_frag_next->next_waiting = NULL;
                    new_frag_next->chunk = (void*)((size_t)current_chunk + size + ALIGN(sizeof(size_t)));
                    if(topchunk_pool->free_metadata != NULL)
                    {
                        metadata *n = topchunk_pool->metadata_allocated;
                        while(n->next != NULL){n = n->next;}
                        n->next = new_frag_next;
                    }
                    else
                    {
                        topchunk_pool->free_metadata = new_frag_next;
                    }
                    topchunk_pool->number_of_elements_allocated++;
                    logfile("[+] %zu bytes allocated @ %p\n └──> ",size,new_frag->chunk);
                    logfile("Chunk @ %p fragmented => new freed chunk created @ %p\n",new_frag,new_frag_next->chunk);
                }
                else
                {
                    /* Le bloc trouvé a la taille parfaite : pas de fragmentation de bloc */
                    /* Priorité aux meta qui attendent */
                    metadata *new_frag;
                    if(topchunk_pool->free_metadata != NULL)
                    {
                        /* Si au moins un meta a été libéré, l'allouer */
                        new_frag = topchunk_pool->free_metadata;
                        if(topchunk_pool->free_metadata->next_waiting != NULL)
                        {
                            /* Si le meta libéré a un suivant, le mettre en tête de liste */
                            topchunk_pool->free_metadata = topchunk_pool->free_metadata->next_waiting;
                        }
                        else
                        {
                            topchunk_pool->free_metadata = NULL;
                        }
                    }
                    else
                    {
                        /* Aucun meta libre n'est trouvé : ajouter le meta à la fin */
                        new_frag = (metadata*)((size_t)current_meta + ALIGN(sizeof(metadata)));
                    }

                    // TODO : canary
                    size_t *canary = (size_t*)((size_t)current_chunk + size);
                    *canary = (size_t)0xaabbccddddccbbaa;
                    new_frag->canary = (size_t)0xdeadbeefcafebabe;
                    new_frag->canary_chunk = (size_t)0xdeadbeefcafebabe;
                    new_frag->free = MY_IS_BUSY;
                    new_frag->size_of_chunk = size;
                    new_frag->next_waiting = NULL;
                    new_frag->next = NULL;
                    new_frag->chunk = current_chunk;
                    /* Si au moins un élément est déjà alloué, mettre le nouveau meta alloué à la suite */
                    if(topchunk_pool->metadata_allocated != NULL)
                    {
                        metadata *n = topchunk_pool->metadata_allocated;
                        while(n->next != NULL){n = n->next;}
                        n->next = new_frag;
                    }
                    else
                    {
                        topchunk_pool->metadata_allocated = new_frag;
                    }
                    topchunk_pool->number_of_elements_allocated++;
                    topchunk_pool->number_of_elements_freed--;
                    logfile("[+] %zu bytes allocated @ %p\n",size,new_frag->chunk);
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
        logfile("*** ERROR *** : mremap for topchunk_pool failed\n");
        perror("mmap meta_pool");
        exit(1);
    }
    topchunk_pool->total_size_metadata = topchunk_pool->total_size_metadata + MY_PAGE_SIZE;
    logfile("[+] Not enough memory for topchunk_pool @ %p : successfully mapped %zu more bytes\n",topchunk_pool,MY_PAGE_SIZE);
}

void get_more_memory_mmap_data(size_t size)
{
    /* Augmenter de size + ALIGN(sizeof(size_t)) pour le canary (aligné sur une page !) */
    size_t new_aligned_size = (size_t)(((topchunk_pool->total_size_data + size + ALIGN(sizeof(size_t))) + (MY_PAGE_SIZE - 1)) & (~(MY_PAGE_SIZE - 1)));
    void *ptr = mremap(data_pool,topchunk_pool->total_size_data, new_aligned_size, MREMAP_MAYMOVE);
    if(topchunk_pool == MAP_FAILED || ptr != data_pool)
    {
        logfile("*** ERROR *** : mremap for data_pool failed\n");
        perror("mmap meta_pool");
        exit(1);
    }
    topchunk_pool->total_size_data = new_aligned_size;
    logfile("[+] Not enough memory for data_pool @ %p : successfully mapped %zu more bytes\n",data_pool,MY_PAGE_SIZE);
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

    /* Si au moins un élément est déjà alloué, mettre le nouveau meta alloué à la suite */
    if(topchunk_pool->metadata_allocated != NULL)
    {
        metadata *n = topchunk_pool->metadata_allocated;
        while(n->next != NULL){n = n->next;}
        n->next = new_meta;
    }
    else
    {
        /* Si aucun élément n'a été alloué depuis */
        topchunk_pool->metadata_allocated = new_meta;
    }

    topchunk_pool->current_size_metadata += ALIGN(sizeof(metadata));
    topchunk_pool->number_of_elements_allocated++;
    // TODO : canary
    // size_t *chunk = new_meta->chunk;
    // chunk[new_meta->size_of_chunk] = (size_t)0xaabbccddddccbbaa;
    size_t *canary = (size_t*)((size_t)new_meta->chunk + new_meta->size_of_chunk);
    
    *canary = (size_t)0xaabbccddddccbbaa;
    logfile("[+] %zu bytes allocated @ %p\n",size,new_meta->chunk);

    return new_meta->chunk;
}

unsigned char find_element_to_free(void *ptr)
{
    metadata *current_meta = topchunk_pool->metadata_allocated;
    void *current_chunk;

    /* Pour chaque élément indépendamment de l'ordre, on regarde s'il existe un meta déjà libéré */
    while(current_meta != NULL)
    {
        //current_meta = (metadata*)((size_t)meta_pool + i*ALIGN(sizeof(metadata))); 
        current_chunk = current_meta->chunk;
        
        if(current_chunk == ptr)
        {
            /* Si le bloc était occupé il devient libre */
            current_meta->free = MY_IS_FREE;

            /* Ajouter à la taille du bloc free le canary de fin */
            current_meta->size_of_chunk += ALIGN(sizeof(size_t));

            /* Si au moins un élément est déjà libéré, mettre le nouveau meta libéré à la suite */
            if(topchunk_pool->free_metadata != NULL)
            {
                /* Si au moins un élément est libéré, faire suivre la meta */
                metadata *n = topchunk_pool->free_metadata;
                while(n->next_waiting != NULL){n = n->next_waiting;}
                n->next_waiting = current_meta;
            }
            else
            {
                /* Si aucun élément n'est libéré, alors initialiser la liste */
                topchunk_pool->free_metadata = current_meta;
            }
            current_meta->next_waiting = NULL;
            if(topchunk_pool->metadata_allocated != NULL)
            {
                /* Si au moins un élément est alloué, supprimer le bloc de la liste et passer à son suivant */
                metadata *_ = topchunk_pool->metadata_allocated;
                metadata *previous = NULL;
                while(_ != NULL)
                {
                    if(_->next == current_meta)   
                    {
                        previous = _;
                        break;
                    }
                    _ = _->next;
                }
                /* Si le bloc avait un suivant alors raccrocher son précédent avec son suivant pour l'enlever de la liste des éléments alloués */
                if(previous != NULL)
                {
                    /* Le bloc avait un meta alloué précédent */
                    if(current_meta->next != NULL)
                    {
                        /* Le bloc avait un meta alloué suivant et précédent */
                        metadata *next_one = current_meta->next;
                        previous->next = next_one;
                    }
                    else
                    {
                        /* Le bloc n'avait qu'un meta alloué précédent */
                        topchunk_pool->metadata_allocated = previous;
                        topchunk_pool->metadata_allocated->next = NULL;
                    }
                }
                else
                {
                    /* Le bloc n'avait pas de meta alloué précédent */
                    if(current_meta->next != NULL)
                    {
                        /* Le bloc n'avait qu'un meta alloué suivant */
                        metadata *next_one = current_meta->next;
                        topchunk_pool->metadata_allocated = next_one;
                        next_one->next = NULL;
                    }
                    else
                    {
                        /* Le bloc n'avait ni de meta alloué suivant ni précédent */
                        topchunk_pool->metadata_allocated = NULL;
                    }
                }
            }
            topchunk_pool->number_of_elements_allocated--;
            topchunk_pool->number_of_elements_freed++;
            return 1;
        }
    current_meta = current_meta->next;
    }
    /* Not found or double free */
    metadata *_ = topchunk_pool->free_metadata;
    while(_ != NULL)
    {
        if(_->chunk == ptr)
        {
            return 2;
        }
        _ = _->next_waiting;
    }
    return 0;
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
            logfile("??? %p is not found in allocated chunks ???\n",ptr);
            break;
        case 1:
            /* found */
            logfile("[+] Memory @ %p successfully freed.\n",ptr);
            break;
        case 2:
            /* double free ! */
            logfile("!!! VULN !!! : Double free detected for %p pointer\n",ptr);
            exit(1);
            break;
    }
}

void *my_calloc(size_t nmemb, size_t size) {
    size_t *ptr = NULL;
    if(nmemb == 0 || size == 0)
    {
        ptr = my_malloc(ALIGNMENT);
        memset(ptr,0,nmemb * size);
        return ptr;
    }

    ptr = my_malloc(nmemb * size);
    memset(ptr,0,nmemb * size);
    logfile("[+] %zu bytes set to 0 @ %p\n",nmemb * size,ptr);
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