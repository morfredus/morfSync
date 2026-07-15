/**
 * paths.h — Emplacement de stockage des données selon les conventions de l'OS.
 *
 * morfSync est un service : ses données (journaux de synchro par domaine)
 * vont à l'endroit attendu par le système, sous l'organisation « morfredus »,
 * et surtout ACCESSIBLE À L'UTILISATEUR (pas /var/lib réservé au root).
 *   - Linux : $XDG_DATA_HOME/morfredus/morfSync, sinon
 *     ~/.local/share/morfredus/morfSync.
 *   - Windows : %LOCALAPPDATA%\morfredus\morfSync, sinon
 *     %ProgramData%\morfredus\morfSync.
 *
 * Utilisé quand la configuration ne fixe pas explicitement `dataDir`. Le service
 * systemd tourne donc en tant que l'utilisateur (User=), pas en DynamicUser.
 */

#pragma once
#include <string>

namespace hsh {

// Répertoire de données par défaut, conforme à l'OS courant.
std::string defaultDataDir();

} // namespace hsh
