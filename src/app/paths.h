/**
 * paths.h — Emplacement de stockage des données du service.
 *
 * morfSync est un service, pas une application utilisateur : ses données
 * (journaux de synchro par domaine) sont sa source de vérité, et personne ne les
 * ouvre à la main. Toutes les opérations (lecture, suppression) passent par les
 * consommateurs via le réseau. C'est de l'ÉTAT PERSISTANT (doctrine morfSystem,
 * docs/FILESYSTEM.md), rangé sous /var/lib, distinct du programme et de la config :
 *   - Linux   : $STATE_DIRECTORY (posé par systemd), sinon /var/lib/morfsystem/morfsync.
 *   - Windows : %ProgramData%\morfsystem\morfsync\state.
 *
 * Utilisé quand la configuration ne fixe pas explicitement `dataDir`. L'unité
 * déclare StateDirectory=morfsystem/morfsync : systemd crée le dossier possédé par
 * le compte du service. main.cpp vérifie la créabilité et l'écriture au démarrage.
 */

#pragma once
#include <string>

namespace hsh {

// Répertoire de données par défaut, conforme à l'OS courant.
std::string defaultDataDir();

} // namespace hsh
