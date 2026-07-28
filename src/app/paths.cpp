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
    // Données MÉTIER du service (journaux de synchro par domaine) : c'est de
    // l'ÉTAT PERSISTANT généré par le service, pas de la config. Il vit donc sous
    // /var/lib, distinct du programme (/opt) et de la config admin (/etc), selon
    // la doctrine du parc (docs/FILESYSTEM.md).
    //
    // Sous systemd, l'unité déclare StateDirectory=morfsystem/morfsync : systemd
    // crée /var/lib/morfsystem/morfsync possédé par le User= du service et
    // l'expose via $STATE_DIRECTORY. On l'utilise en priorité (aucun problème de
    // droits à rattraper) ; la variable peut lister plusieurs chemins séparés par
    // ':', le premier est la racine.
    if (std::string sd = env("STATE_DIRECTORY"); !sd.empty()) {
        const std::string first = sd.substr(0, sd.find(':'));
        if (!first.empty()) return first;
    }
#if defined(_WIN32)
    if (std::string pd = env("PROGRAMDATA"); !pd.empty())   return pd + "\\morfsystem\\morfsync\\state";
    return "data";
#else
    return "/var/lib/morfsystem/morfsync";
#endif
}

} // namespace hsh
