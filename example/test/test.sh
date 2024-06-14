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

# Affiche les variables d'environnement pour le débogage
echo "MSM_OUTPUT=$MSM_OUTPUT"
echo "LD_PRELOAD=$LD_PRELOAD"

# Exécute la commande passée en argument
"$@"

# Affiche un message de fin
echo "Command executed: $@"

# Annule la définition de LD_PRELOAD
unset LD_PRELOAD
