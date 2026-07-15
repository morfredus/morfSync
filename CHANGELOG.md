# Changelog

Toutes les évolutions notables de HomeServerHub sont consignées ici.
Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) ;
versionnage [SemVer](https://semver.org/lang/fr/).

## [Non publié]

## [0.2.0] — 2026-07-15
### Ajouté
- Stockage des données à l'emplacement conforme à l'OS quand `dataDir` n'est pas
  précisé : `$STATE_DIRECTORY` (service systemd) puis XDG sous Linux,
  `%ProgramData%\HomeServerHub` sous Windows (`src/app/paths.*`).
- Préchargement des journaux existants au démarrage (les données survivent aux
  redémarrages, reflétées immédiatement) et journal de démarrage détaillé (flushé).
- Endpoint `GET /api/status` : domaines connus, nombre d'entités, curseur `lastSeq`.
- Scripts d'installation en service / démarrage automatique :
  `scripts/linux/install-service.sh` (systemd) et
  `scripts/windows/install-service.ps1` (tâche planifiée + règle de pare-feu).
- Documentation : `docs/INSTALLATION.md` (installation + configuration de
  l'écoute sous Linux et Windows) et `docs/GUIDE_DEBUTANT.md` (prise en main
  pas à pas, avec test de communication entre deux machines).

### Modifié
- PUSH **idempotent** : un renvoi à l'identique (même `rev` + `updatedAt`) est un
  no-op sans nouveau `seq`, et une version plus ancienne ne peut plus écraser une
  plus récente — résolution déterministe *highest-rev puis updatedAt* (au lieu du
  last-write-wins par arrivée). Permet au client de re-pousser sans générer de
  bruit ; combiné au PUSH incrémental côté ComponentHub, la synchro passe à
  l'échelle.

## [0.1.0] — 2026-07-15
### Ajouté
- Scaffold initial du socle de synchronisation offline-first de l'écosystème *morf*.
- Serveur HTTP/1.1 minimal, autonome et cross-plateforme (winsock2 / sockets
  POSIX), sans framework web. Seule dépendance externe : `nlohmann_json`.
- Journal de changements ordonné par domaine (`ChangeStore`) : numéro de séquence
  monotone, last-write-wins par `id`, tombstones, détection d'écrasement,
  pagination, persistance JSON atomique.
- Endpoints du contrat : `GET /api/health`, `GET /api/{domaine}/changes`,
  `POST /api/{domaine}/changes`.
- Authentification optionnelle par jeton partagé (`Bearer`).
- Profils de compilation win-x64 / Linux / ARM64 (natif et croisé), alignés sur
  ComponentHub (CMake + presets + toolchain aarch64).
- Test de fumée headless du journal (option `HSH_BUILD_SMOKE`).
- Contrat de synchronisation versionné dans `docs/sync-contract.md`.
- Service systemd et README bilingue (EN/FR).

[Non publié]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/morfredus/HomeServerHub_travail/releases/tag/v0.1.0
