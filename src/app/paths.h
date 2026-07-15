/**
 * paths.h — Emplacement de stockage des données selon les conventions de l'OS.
 *
 * HomeServerHub est un service : ses données (journaux de synchro par domaine)
 * doivent aller à l'endroit attendu par le système, pas dans le dossier courant.
 *   - Linux (service systemd) : $STATE_DIRECTORY (fourni par StateDirectory=),
 *     sinon XDG ($XDG_DATA_HOME ou ~/.local/share/homeserverhub).
 *   - Windows : %ProgramData%\HomeServerHub (partagé, adapté à un service SYSTEM),
 *     sinon %LOCALAPPDATA%\HomeServerHub.
 *
 * Utilisé quand la configuration ne fixe pas explicitement `dataDir`.
 */

#pragma once
#include <string>

namespace hsh {

// Répertoire de données par défaut, conforme à l'OS courant.
std::string defaultDataDir();

} // namespace hsh
