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

// Taille initiale des pools
#define INITIAL_DATA_POOL_SIZE (1024 * 1024) // 1 Mo
#define INITIAL_META_POOL_SIZE (1024 * 1024) // 1 Mo

// Alignement des blocs de mémoire
#define ALIGNMENT 8

// Aligner la taille sur la taille de l'alignement
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

// Fonction d'initialisation des pools de mémoire
static void init_pools() {
    // Allouer le pool de données en utilisant mmap
    // NULL: le système choisit l'adresse de début
    // INITIAL_DATA_POOL_SIZE: taille de la région de mémoire (1 Mo)
    // PROT_READ | PROT_WRITE: mémoire lisible et modifiable
    // MAP_PRIVATE | MAP_ANONYMOUS: mémoire privée et anonyme (pas associée à un fichier)
    // -1 et 0: ignorés car MAP_ANONYMOUS est spécifié
    data_pool = mmap(NULL, INITIAL_DATA_POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // Vérifier si mmap a échoué pour le pool de données
    if (data_pool == MAP_FAILED) {
        // Afficher un message d'erreur et terminer le programme
        perror("mmap data_pool");
        exit(EXIT_FAILURE);
    }
    // Définir la taille du pool de données
    data_pool_size = INITIAL_DATA_POOL_SIZE;

    // Allouer le pool de métadonnées en utilisant mmap
    // Les paramètres sont similaires à ceux utilisés pour le pool de données
    meta_pool = mmap(NULL, INITIAL_META_POOL_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    // Vérifier si mmap a échoué pour le pool de métadonnées
    if (meta_pool == MAP_FAILED) {
        // Afficher un message d'erreur et terminer le programme
        perror("mmap meta_pool");
        exit(EXIT_FAILURE);
    }
    // Définir la taille du pool de métadonnées
    meta_pool_size = INITIAL_META_POOL_SIZE;

    // Initialiser le premier descripteur de bloc dans le pool de métadonnées
    // Indique que toute la taille du pool de données est disponible pour l'allocation
    meta_pool->size = data_pool_size;
    // Marquer le bloc comme libre (1 signifie libre, 0 signifie occupé)
    meta_pool->free = 1;
    // Pas d'autres descripteurs de bloc, donc le champ next est défini sur NULL
    meta_pool->next = NULL;
}


// Fonction malloc sécurisée pour allouer de la mémoire dans le pool géré manuellement
void *my_malloc(size_t size) {
    // Aligner la taille demandée sur un multiple de l'alignement défini pour éviter le "fragmentation interne"
    size = ALIGN(size);

    // Initialiser les pools de mémoire si ce n'est pas déjà fait
    if (!data_pool || !meta_pool) {
        init_pools(); // Appelle la fonction d'initialisation des pools de mémoire
    }

    // Parcourir la liste des descripteurs pour trouver un bloc de mémoire libre suffisamment grand
    block_meta_t *current = meta_pool;
    while (current) {
        // Vérifier si le bloc courant est libre et si sa taille est suffisante
        if (current->free && current->size >= size) {
            // Calculer l'espace restant après l'allocation de la taille demandée
            size_t remaining_size = current->size - size - sizeof(block_meta_t);
            // Vérifier s'il reste assez de place pour créer un nouveau descripteur de bloc après l'allocation
            if (remaining_size > sizeof(block_meta_t)) {
                // Créer un nouveau descripteur pour le bloc de mémoire restant après celui qui sera alloué
                block_meta_t *new_meta = (block_meta_t *)((char *)current + sizeof(block_meta_t) + size);
                new_meta->size = remaining_size;
                new_meta->free = 1; // Marquer le nouveau bloc comme libre
                new_meta->next = current->next; // Insérer le nouveau bloc dans la liste
                current->size = size; // Ajuster la taille du bloc courant à la taille demandée
                current->next = new_meta; // Faire pointer le bloc courant vers le nouveau bloc
            }
            // Marquer le bloc courant comme occupé
            current->free = 0;
            // Retourner l'adresse du début du bloc de données, juste après le descripteur
            return (char *)current + sizeof(block_meta_t);
        }
        // Passer au descripteur de bloc suivant
        current = current->next;
    }

    // Si aucun bloc adéquat n'est trouvé, il faudrait normalement agrandir le pool de mémoire
    // Ceci n'est pas implémenté ici; une implémentation pourrait utiliser mremap pour agrandir le pool
    // Retourner NULL pour indiquer l'échec de l'allocation
    return NULL;
}


// Fonction de libération de mémoire sécurisée
void my_free(void *ptr) {
    // Si le pointeur est NULL, ne rien faire
    if (!ptr) return;

    // Obtenir le descripteur de bloc correspondant au bloc de données à partir du pointeur
    // Le descripteur se trouve juste avant l'adresse du bloc de données
    block_meta_t *meta = (block_meta_t *)((char *)ptr - sizeof(block_meta_t));
    // Marquer le bloc comme libre
    meta->free = 1;

    // Fusionner les blocs libres consécutifs pour éviter la fragmentation
    block_meta_t *current = meta_pool;
    while (current) {
        // Si le bloc courant et le bloc suivant sont tous deux libres, les fusionner
        if (current->free && current->next && current->next->free) {
            // Ajouter la taille du descripteur suivant et du bloc suivant au bloc courant
            current->size += sizeof(block_meta_t) + current->next->size;
            // Faire pointer le bloc courant vers le bloc suivant du bloc fusionné
            current->next = current->next->next;
        }
        // Passer au descripteur de bloc suivant
        current = current->next;
    }
}


// Fonction calloc sécurisée pour allouer et initialiser de la mémoire
void *my_calloc(size_t nmemb, size_t size) {
    // Calculer la taille totale à allouer
    size_t total_size = nmemb * size;
    // Utiliser my_malloc pour allouer la mémoire
    void *ptr = my_malloc(total_size);
    // Si l'allocation réussit, initialiser la mémoire allouée à zéro
    if (ptr) {
        memset(ptr, 0, total_size);
    }
    // Retourner le pointeur vers la mémoire allouée et initialisée
    return ptr;
}


// Fonction realloc sécurisée pour redimensionner un bloc de mémoire alloué
void *my_realloc(void *ptr, size_t size) {
    // Si le pointeur est NULL, allouer un nouveau bloc de mémoire
    if (!ptr) return my_malloc(size);
    // Si la taille est zéro, libérer le bloc de mémoire et retourner NULL
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    // Obtenir le descripteur de bloc correspondant au bloc de données
    block_meta_t *meta = (block_meta_t *)((char *)ptr - sizeof(block_meta_t));
    // Si la taille actuelle du bloc est suffisante, retourner le même pointeur
    if (meta->size >= size) {
        return ptr;
    }

    // Allouer un nouveau bloc de mémoire avec la nouvelle taille
    void *new_ptr = my_malloc(size);
    // Si l'allocation réussit, copier les données de l'ancien bloc vers le nouveau
    if (new_ptr) {
        memcpy(new_ptr, ptr, meta->size);
        // Libérer l'ancien bloc de mémoire
        my_free(ptr);
    }
    // Retourner le pointeur vers le nouveau bloc de mémoire
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
