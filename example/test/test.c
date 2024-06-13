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
        [PAS OK]        enchaînement de malloc de différentes tailles et de free, sans mélanger les appels de malloc et free
*/
int main(void)
{
    void *ptr1 = my_malloc(0);
    void *ptr2 = my_malloc(4096 * 1000 + 1);
    void *ptr3 = my_malloc(1);
    void *ptr4 = my_malloc(4096 * 1000 + 1);
    my_free(ptr1);
    my_free(ptr2);
    my_free(ptr3);
    my_free(ptr4);
}