cmake_minimum_required(VERSION 3.21)

# Version lue depuis le fichier VERSION à la racine (même convention que ComponentHub).
set(HSH_VERSION_FILE "${CMAKE_SOURCE_DIR}/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HSH_VERSION_FILE}")
file(STRINGS "${HSH_VERSION_FILE}" HSH_VERSION LIMIT_COUNT 1)
string(STRIP "${HSH_VERSION}" HSH_VERSION)

project(HomeServerHub VERSION "${HSH_VERSION}" LANGUAGES CXX)

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
add_library(homeserverhub_core STATIC
    src/net/http_server.cpp
    src/sync/change_store.cpp
    src/app/config.cpp
    src/app/paths.cpp
)
target_include_directories(homeserverhub_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(homeserverhub_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads)
if(WIN32)
    target_link_libraries(homeserverhub_core PUBLIC ws2_32)   # winsock2
endif()
if(MSVC)
    target_compile_options(homeserverhub_core PRIVATE /utf-8)
endif()

# =====================================================================
# Exécutable serveur.
# =====================================================================
add_executable(HomeServerHub src/main.cpp)
target_link_libraries(HomeServerHub PRIVATE homeserverhub_core)
target_compile_definitions(HomeServerHub PRIVATE HSH_VERSION="${HSH_VERSION}")
if(MSVC)
    target_compile_options(HomeServerHub PRIVATE /utf-8)
endif()

# =====================================================================
# Test de fumée headless (aucun réseau), activé sur demande.
#   cmake --preset linux -DHSH_BUILD_SMOKE=ON && ctest --preset linux
# =====================================================================
option(HSH_BUILD_SMOKE "Construire le test de fumée du journal" OFF)
if(HSH_BUILD_SMOKE)
    enable_testing()
    add_executable(hsh_smoke test/smoke_main.cpp)
    target_link_libraries(hsh_smoke PRIVATE homeserverhub_core)
    add_test(NAME smoke COMMAND hsh_smoke)
endif()
