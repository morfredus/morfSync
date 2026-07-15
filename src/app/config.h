/**
 * config.h — Configuration du serveur, lue depuis config.json.
 *
 * config.json contient le jeton d'authentification : il est gitignore
 * (voir .gitignore). Un modèle sans secret est fourni : config.example.json.
 */

#pragma once
#include <string>

namespace hsh {

struct Config {
    std::string host = "0.0.0.0";      // écoute sur toutes les interfaces (LAN)
    unsigned short port = 8080;
    std::string dataDir;               // vide = emplacement par défaut selon l'OS (voir paths.h)
    std::string token;                 // Bearer partagé ; vide = auth désactivée (LAN de confiance)

    // Charge depuis un fichier JSON. Les clés absentes gardent leur défaut.
    // Retourne false seulement si le fichier existe mais est illisible/invalide.
    static bool loadFromFile(const std::string& path, Config& out, std::string& error);
};

} // namespace hsh
