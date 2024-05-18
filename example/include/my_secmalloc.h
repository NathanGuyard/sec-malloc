#ifndef _SECMALLOC_H
#define _SECMALLOC_H

#include <stddef.h>  // Inclut les définitions de types standard comme size_t

// Déclaration des fonctions d'allocateur personnalisées

// Fonction pour allouer de la mémoire
void *malloc(size_t size);

// Fonction pour libérer de la mémoire
void free(void *ptr);

// Fonction pour allouer et initialiser de la mémoire
void *calloc(size_t nmemb, size_t size);

// Fonction pour redimensionner un bloc de mémoire alloué
void *realloc(void *ptr, size_t size);

#endif