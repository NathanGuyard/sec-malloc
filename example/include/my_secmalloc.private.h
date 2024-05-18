#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H

#include "my_secmalloc.h"  // Inclut les définitions de base pour l'allocateur

// Structure de descripteur de bloc
typedef struct block_meta {
    size_t size;           // Taille du bloc de mémoire
    int free;              // Indique si le bloc est libre (1 = libre, 0 = occupé)
    struct block_meta *next; // Pointeur vers le prochain descripteur de bloc dans la liste
} block_meta_t;

// Variables globales pour les pools de mémoire
extern void *data_pool;           // Pointeur vers le pool de mémoire de données
extern block_meta_t *meta_pool;   // Pointeur vers le pool de mémoire de métadonnées
extern size_t data_pool_size;     // Taille du pool de mémoire de données
extern size_t meta_pool_size;     // Taille du pool de mémoire de métadonnées

// Fonctions d'allocateur
void    *my_malloc(size_t size);   // Fonction pour allouer de la mémoire
void    my_free(void *ptr);        // Fonction pour libérer de la mémoire
void    *my_calloc(size_t nmemb, size_t size);  // Fonction pour allouer et initialiser de la mémoire
void    *my_realloc(void *ptr, size_t size);    // Fonction pour redimensionner un bloc de mémoire

#endif