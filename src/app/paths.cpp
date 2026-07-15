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
#if defined(_WIN32)
    if (std::string pd = env("PROGRAMDATA"); !pd.empty())   return pd + "\\HomeServerHub";
    if (std::string la = env("LOCALAPPDATA"); !la.empty())  return la + "\\HomeServerHub";
    return "data";
#else
    // systemd StateDirectory= : le service reçoit un dossier dédié géré par l'OS.
    if (std::string sd = env("STATE_DIRECTORY"); !sd.empty()) {
        const std::size_t colon = sd.find(':');   // peut lister plusieurs chemins
        return colon == std::string::npos ? sd : sd.substr(0, colon);
    }
    if (std::string xdg = env("XDG_DATA_HOME"); !xdg.empty()) return xdg + "/homeserverhub";
    if (std::string home = env("HOME"); !home.empty())        return home + "/.local/share/homeserverhub";
    return "data";
#endif
}

} // namespace hsh
