# Toolchain CMake : cross-compilation vers Linux ARM64 (aarch64), pour le
# preset "linux-arm64-cross" (voir CMakePresets.json).
#
# morfSync n'a qu'une dépendance externe (nlohmann_json, en-têtes seuls),
# donc la cross-compilation est bien plus légère que celle de ComponentHub (Qt).
#
# Prérequis côté machine hôte :
#   - une toolchain croisée "aarch64-linux-gnu-" (gcc/g++) ;
#   - nlohmann-json disponible pour la cible (en-têtes dans le sysroot, ou
#     paquet système si l'on compile directement sur le Raspberry via le
#     preset "linux-arm64").
#
# Variables d'environnement (optionnelles) :
#   MS_CROSS_PREFIX : préfixe des outils (défaut : "aarch64-linux-gnu-")
#   MS_SYSROOT      : chemin du sysroot cible

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

if(DEFINED ENV{MS_CROSS_PREFIX})
    set(_ms_prefix "$ENV{MS_CROSS_PREFIX}")
else()
    set(_ms_prefix "aarch64-linux-gnu-")
endif()

set(CMAKE_C_COMPILER   "${_ms_prefix}gcc")
set(CMAKE_CXX_COMPILER "${_ms_prefix}g++")

if(DEFINED ENV{MS_SYSROOT})
    set(CMAKE_SYSROOT "$ENV{MS_SYSROOT}")
    list(APPEND CMAKE_FIND_ROOT_PATH "$ENV{MS_SYSROOT}")
endif()

# Outils cherchés sur l'HÔTE ; libs/headers/paquets dans la CIBLE.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
