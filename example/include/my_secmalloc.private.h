#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H

#include "my_secmalloc.h"

#define MY_IS_FREE (size_t)0
#define MY_IS_BUSY (size_t)1

typedef struct metadata {
    size_t canary_chunk;           // canary qui compare celui du chunk [ A PLACER A LA FIN D'UN BLOC ]
    size_t size_of_chunk;          // taille du chunk lié
    size_t free;                   // libre ou occuppé 
                                    // si le chunk est libre ET présent dans free_metadata alors il est en attente de se lier à un chunk
    void *chunk;                   // pointeur vers le chunk lié avec le canary à la fin !
    struct metadata *next;         // pointeur vers la metadata suivante qui est allouée
    struct metadata *next_waiting; // pointeur vers la metadata suivante qui est en attente de se lier
    size_t canary;                  // canary qui sera comparé à celui du top_chunk
}metadata;

typedef struct topchunk
{
    size_t canary;                    // canary qui compare les metadata (le même pour chacun des metadata)
    size_t total_size_metadata;       // taille totale des metadata
    size_t current_size_metadata;       // taille courrante des metadata
    size_t total_size_data;           // taille totale des data 
    size_t current_size_data;           // taille courrante des data 
    size_t number_of_elements_allocated;  // nombre d'éléments alloués
    size_t number_of_elements_freed;       // nombre d'éléments libérés
    metadata *free_metadata;             // liste des metadatas qui ont servi à free
    metadata *metadata_allocated;   // pointeur vers dernier metadata alloué
}topchunk;

extern metadata *meta_pool;
extern topchunk *topchunk_pool;

void    *my_malloc(size_t size);   
void    my_free(void *ptr);        
void    *my_calloc(size_t nmemb, size_t size); 
void    *my_realloc(void *ptr, size_t size);  

#endif