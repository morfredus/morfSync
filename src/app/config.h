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

    // Annonce de présence sur le LAN (protocole morfbeacon/1). Activée par
    // défaut : un service que personne ne découvre oblige à le configurer à la
    // main partout, ce que la découverte déclarative existe pour éviter.
    bool beaconEnabled = true;
    unsigned short beaconPort = 45454;   // port du parc, voir ecosystem.json
    int beaconIntervalMs = 15000;

    // Charge depuis un fichier JSON. Les clés absentes gardent leur défaut.
    // Retourne false seulement si le fichier existe mais est illisible/invalide.
    static bool loadFromFile(const std::string& path, Config& out, std::string& error);
};

} // namespace hsh
