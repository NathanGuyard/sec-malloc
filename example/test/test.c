#include <criterion/criterion.h>  
#include "my_secmalloc.private.h"
#include <sys/mman.h>  
#include <stdio.h>

/*
    Résultat des tests :
        [OK]        un malloc 
        [OK]        un malloc suivi d'un free
        [OK]        un malloc supérieur à 4 Mo  
        [OK]        un malloc supérieur à 4 Mo suivi d'un free
        [OK]        fusion des blocs de différentes tailles
        [PAS OK]    fragmentation de blocs de différentes tailles
        [OK]        enchaînement de malloc de différentes tailles et de free, sans mélanger les appels de malloc et free
        [OK]        enchaînement de malloc de différentes tailles et de free, avec mélange des appels de malloc et free
*/

int main(void)
{
    void *ptr1 = my_malloc(64);
    my_free(ptr1);
    void *ptr2 = my_malloc(64);
    void *ptr3 = my_malloc(0);
    void *ptr4 = my_malloc(640);
    my_free(ptr4);
    ptr4 = my_malloc(50000);
    my_free(ptr4);
    my_free(ptr3);
    my_free(ptr2);
    ptr2 = malloc(0);
    my_free(ptr2);
    ptr2 = malloc(0);
    my_free(ptr2);
    ptr2 = malloc(800000);
}

/*
malloc(64) -> 0x2710048 (0x40) (busy)
free().... (free)

*/