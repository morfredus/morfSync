#include "config.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace hsh {

bool Config::loadFromFile(const std::string& path, Config& out, std::string& error) {
    std::ifstream in(path);
    if (!in.is_open()) {
        // Absence de config = on tourne sur les valeurs par défaut. Pas une erreur.
        return true;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        error = std::string("config.json invalide : ") + e.what();
        return false;
    }
    out.host    = j.value("host", out.host);
    out.port    = j.value("port", out.port);
    out.dataDir = j.value("dataDir", out.dataDir);
    out.token   = j.value("token", out.token);
    return true;
}

} // namespace hsh
