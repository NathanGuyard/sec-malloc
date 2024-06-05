#!/bin/bash
# test_with_ld_preload.sh

export LD_PRELOAD="/home/school/Desktop/my_alloc/example/libmy_secmalloc.so"
command_to_test="$1"  # Prend une commande comme argument

# Exécute la commande en utilisant votre bibliothèque dynamique
$command_to_test

# Annule le LD_PRELOAD après l'exécution
unset LD_PRELOAD
