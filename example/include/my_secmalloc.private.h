#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H

#include "my_secmalloc.h"

#define MY_IS_FREE (size_t)0
#define MY_IS_BUSY (size_t)1
#define MY_IS_NOT_HERE (size_t)2

typedef struct metadata {
    size_t canary_chunk;           // canary qui compare celui du chunk [ A PLACER A LA FIN D'UN BLOC ]
    size_t size_of_chunk;          // taille du chunk lié
    size_t free;                   // libre ou occuppé ou metadata retiré de la liste
                                   // MY_IS_NOT_HERE ne fait rien et passe au pointeur suivant
    void *chunk;                   // pointeur vers le chunk lié avec le canary à la fin !
    struct metadata *next;         // pointeur vers la metadata suivante
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
    metadata *free_metadata;             // liste de metadata qui ont servi à free
}topchunk;

extern metadata *meta_pool;
extern topchunk *topchunk_pool;

void    *my_malloc(size_t size);   
void    my_free(void *ptr);        
void    *my_calloc(size_t nmemb, size_t size); 
void    *my_realloc(void *ptr, size_t size);  

#endif