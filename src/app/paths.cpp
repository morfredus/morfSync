#include "paths.h"

#include <cstdlib>

namespace hsh {

namespace {
// getenv encapsulé : renvoie une chaîne vide si la variable est absente.
std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}
} // namespace

std::string defaultDataDir() {
    // Données MÉTIER du service (source de vérité), rangées selon la convention
    // morfSystem : <app_dir>/data (docs/FILESYSTEM.md). morfSync est un service,
    // pas une application utilisateur : personne n'ouvre ces fichiers à la main,
    // toutes les opérations (lecture, suppression) passent par les consommateurs
    // via le réseau. Les données vivent donc sous /opt, pas dans le home.
#if defined(_WIN32)
    if (std::string pd = env("PROGRAMDATA"); !pd.empty())   return pd + "\\morfsync\\data";
    return "data";
#else
    return "/opt/morfsync/data";
#endif
}

} // namespace hsh
