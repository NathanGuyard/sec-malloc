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
    void *A = my_calloc(1,16);   
    my_free(A);
    void *B = my_calloc(6,12);
    void *C = my_malloc(0);
    my_free(B);
    B = my_malloc(600);
    my_free(C);
    my_free(B);
    A = my_calloc(8,5);
    B = my_malloc(0);
    C = my_malloc(0);
    my_free(B);
    my_free(C);
    my_free(A);
    int d = 4;
    my_free(&d);
    my_free(B);
}