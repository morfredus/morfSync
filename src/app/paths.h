/**
 * paths.h — Emplacement de stockage des données du service.
 *
 * morfSync est un service, pas une application utilisateur : ses données
 * (journaux de synchro par domaine) sont sa source de vérité, et personne ne les
 * ouvre à la main. Toutes les opérations (lecture, suppression) passent par les
 * consommateurs via le réseau. Elles suivent donc la convention morfSystem
 * (docs/FILESYSTEM.md), sous <app_dir>/data :
 *   - Linux   : /opt/morfsync/data.
 *   - Windows : %ProgramData%\morfsync\data.
 *
 * Utilisé quand la configuration ne fixe pas explicitement `dataDir`. Le service
 * systemd doit tourner sous un compte ayant l'écriture sur ce dossier (provisionné
 * par morfdeploy) ; main.cpp vérifie la créabilité et l'écriture au démarrage.
 */

#pragma once
#include <string>

namespace hsh {

// Répertoire de données par défaut, conforme à l'OS courant.
std::string defaultDataDir();

} // namespace hsh
