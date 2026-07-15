# morfSync

*Lire dans une autre langue : [English](README.md) · **Français** (ce document).*

![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Raspberry%20Pi-lightgrey)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Build](https://img.shields.io/badge/CMake-3.21+-064F8C?logo=cmake)
![License](https://img.shields.io/badge/License-GPL--3.0--only-blue)

**morfSync** est le socle de synchronisation *offline-first* de l'écosystème
*morf* (ComponentHub, MeteoHub, RaspberryDashboard, SiteWatch…). Ce n'est **pas**
une base de données. Chaque client garde sa propre copie locale des données et
travaille dessus sans réseau. Le hub ne sert qu'à maintenir toutes les copies
cohérentes sur le réseau local.

> Une seule architecture pour tous les projets : le même **contrat** de synchro
> (enveloppe + endpoints REST) vaut pour une application de bureau comme pour un
> ESP32 à mémoire contrainte.

## Fonctionnement (en un paragraphe)

Chaque entité synchronisable porte une enveloppe — `{ id (UUID), rev, updatedAt,
deleted, origin }` — autour d'une charge utile métier opaque. Les clients
**PUSH**ent leurs changements locaux, puis **PULL**ent tout depuis leur curseur
(`GET /changes?since=N`). Le hub attribue un **numéro de séquence monotone** qui
ordonne tout et arbitre les conflits en *last-write-wins* — il **ne compare
jamais les horloges** des machines (l'ESP32 dérive, le Raspberry Pi n'a pas de
RTC). La spécification complète est dans [docs/sync-contract.md](docs/sync-contract.md).

## API

| Méthode | Endpoint | Rôle |
|---------|----------|------|
| `GET`  | `/api/health` | État du serveur (ouvert, sans auth) |
| `GET`  | `/api/status` | Domaines connus : entités, curseur `lastSeq` et `journalId` (époque) |
| `GET`  | `/api/{domaine}/changes?since=N&limit=M` | PULL des changements depuis le curseur |
| `POST` | `/api/{domaine}/changes` | PUSH des changements locaux, récupère le `seq` attribué |

`{domaine}` = un journal par projet (`componenthub`, `meteohub`…), créé à la
demande. Auth optionnelle par jeton partagé via `Authorization: Bearer <token>`.

## Choix de conception

- **Aucun framework web.** Un serveur HTTP/1.1 minimal et autonome (winsock2 /
  sockets POSIX). Seule dépendance externe : `nlohmann_json` — celle que
  ComponentHub utilise déjà. Compile indépendamment sur win-x64, Linux et ARM64.
- **Agnostique au métier.** Le hub transporte des enveloppes ; il n'interprète
  jamais la charge `data`. Ajouter un projet ne demande aucune modification serveur.
- **Abstraction de stockage prête.** Les journaux sont des fichiers JSON
  aujourd'hui ; un futur backend SQLite les remplacera sans toucher aux clients.

## Compilation

Nécessite CMake ≥ 3.21, un compilateur C++17 et `nlohmann_json`.

### Windows (MSYS2 / MinGW)
```bash
pacman -S mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
cmake --preset mingw
cmake --build --preset mingw
# -> build-mingw/morfSync.exe
```

### Linux (x86_64) / Raspberry Pi (ARM64, natif)
```bash
sudo apt install nlohmann-json3-dev cmake ninja-build
cmake --preset linux           # ou : --preset linux-arm64 directement sur le Pi
cmake --build --preset linux
# -> build/morfSync
```

### Linux ARM64 (croisé depuis x86_64)
```bash
export MS_SYSROOT=/chemin/vers/sysroot/arm64   # doit fournir les en-têtes nlohmann_json
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
```

### Lancer les tests de fumée (sans réseau)
```bash
cmake --preset linux -DMS_BUILD_SMOKE=ON
cmake --build --preset linux
ctest --preset linux
```

## Exécution

```bash
cp config.example.json config.json    # puis renseigner un token si souhaité
./morfSync                       # ou : ./morfSync /chemin/config.json
```
Vérification rapide : `curl http://localhost:8080/api/health`

**Installation en service / démarrage automatique** (Linux et Windows),
configuration de l'écoute (adresse/port), pare-feu : voir
[docs/INSTALLATION.md](docs/INSTALLATION.md). Prise en main pas à pas pour
débuter : [docs/GUIDE_DEBUTANT.md](docs/GUIDE_DEBUTANT.md).

- Linux : `sudo ./scripts/linux/install-service.sh`
- Windows (PowerShell admin) : `.\scripts\windows\install-service.ps1`

## Organisation du dépôt

```
src/net/      serveur HTTP minimal cross-plateforme
src/sync/     enveloppe de changement + journal ordonné (ChangeStore)
src/app/      configuration
src/main.cpp  câble les endpoints du contrat au journal
docs/         le contrat de synchronisation (source de vérité)
cmake/        toolchain de cross-compilation ARM64
scripts/      packaging / fichiers de service
test/         test de fumée headless
```

## Licence

GPL-3.0-only — voir [LICENSE](LICENSE).
