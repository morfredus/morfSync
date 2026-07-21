cmake_minimum_required(VERSION 3.21)

# Version lue depuis le fichier VERSION à la racine (même convention que ComponentHub).
set(MS_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MS_VERSION_FILE}")
file(STRINGS "${MS_VERSION_FILE}" MS_VERSION LIMIT_COUNT 1)
string(STRIP "${MS_VERSION}" MS_VERSION)

project(morfSync VERSION "${MS_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dépendances ----------------------------------------------------------
# Seule dépendance externe : nlohmann_json (déjà utilisée par ComponentHub).
# Fournie par le système : MSYS2 (pacman), apt sous Linux, sysroot en cross.
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

# =====================================================================
# Cœur portable : serveur HTTP minimal + journal de synchro + config.
# Aucune dépendance à un framework web. Réutilisable par des tests/outils.
# =====================================================================
add_library(morfsync_core STATIC
    src/net/http_server.cpp
    src/net/beacon.cpp
    src/sync/change_store.cpp
    src/app/config.cpp
    src/app/paths.cpp
)
target_include_directories(morfsync_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(morfsync_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads)
if(WIN32)
    target_link_libraries(morfsync_core PUBLIC ws2_32)   # winsock2
endif()
if(MSVC)
    target_compile_options(morfsync_core PRIVATE /utf-8)
endif()

# =====================================================================
# Exécutable serveur (binaire : morfSync).
# =====================================================================
add_executable(morfSync src/main.cpp)
target_link_libraries(morfSync PRIVATE morfsync_core)
target_compile_definitions(morfSync PRIVATE MS_VERSION="${MS_VERSION}")
if(MSVC)
    target_compile_options(morfSync PRIVATE /utf-8)
endif()

# =====================================================================
# Test de fumée headless (aucun réseau), activé sur demande.
#   cmake --preset linux -DMS_BUILD_SMOKE=ON && ctest --preset linux
# =====================================================================
option(MS_BUILD_SMOKE "Construire le test de fumée du journal" OFF)
if(MS_BUILD_SMOKE)
    enable_testing()
    add_executable(ms_smoke test/smoke_main.cpp)
    target_link_libraries(ms_smoke PRIVATE morfsync_core)
    add_test(NAME smoke COMMAND ms_smoke)
endif()
