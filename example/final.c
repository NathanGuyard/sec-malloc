#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void main(void)
{
    // Allouer deux blocs de mémoire, puis libérer le deuxième bloc
    char *ptr1 = malloc(100);
    char *ptr2 = malloc(200);

    // Remplir le premier bloc avec des données
    strcpy(ptr1, "This is a test for realloc optimization.");

    // Libérer le deuxième bloc
    free(ptr2);

    // Réallouer le premier bloc avec une taille qui peut être satisfaite par la fusion avec le deuxième bloc
    char *new_ptr = realloc(ptr1, 250);

    // Libérer le bloc réalloué
    free(new_ptr);
}
