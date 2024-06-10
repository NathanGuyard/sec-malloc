#!/bin/bash

# Vérifie que l'argument est fourni
if [ -z "$1" ]; then
  echo "Usage: $0 <command>"
  exit 1
fi

# Définit la variable d'environnement MSM_OUTPUT
export MSM_OUTPUT="/home/school/Desktop/my_alloc/example/report_file.log"

# Définit LD_PRELOAD pour utiliser votre bibliothèque
export LD_PRELOAD="/home/school/Desktop/my_alloc/example/libmy_secmalloc.so"

# Exécute la commande passée en argument via gdb
gdb --batch --ex "run" --ex "bt" --ex "quit" --args "$@"

# Annule la définition de LD_PRELOAD
unset LD_PRELOAD
