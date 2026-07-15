# Changelog

Toutes les évolutions notables de HomeServerHub sont consignées ici.
Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) ;
versionnage [SemVer](https://semver.org/lang/fr/).

## [Non publié]

## [0.2.7] — 2026-07-15
### Corrigé
- `update-service.sh --build` **choisit le bon preset selon l'architecture** :
  `linux-arm64` (dossier `build-arm64`) sur un Raspberry Pi 64 bits, `linux`
  sinon. Auparavant le preset `linux` était codé en dur. Le preset peut aussi
  être forcé en argument (`--build linux-arm64-cross`). Documentation de mise à
  jour ajustée en conséquence.

## [0.2.6] — 2026-07-15
### Ajouté
- Option **`--version` / `-v`** : affiche la version sans démarrer le serveur
  (vérification fiable — le nom et le numéro sont stockés séparément dans le
  binaire, donc introuvables par un simple `grep`).

### Corrigé
- `update-service.sh --build` fait désormais **`git pull` puis une
  reconstruction propre** (`rm -rf build`) : après un simple `git pull`, CMake ne
  recompilait pas toujours et l'ancien binaire (version périmée) était recopié.
  La mise à jour du service ne remplaçait alors rien.
- Documentation : procédure de mise à jour **complète et fiable** dans
  `docs/INSTALLATION.md` (build neuf + vérification via `--version`).

## [0.2.5] — 2026-07-15
### Ajouté
- **Identité de journal (« époque ») par domaine** : un `journalId` stable est
  généré à la création d'un journal et exposé dans la réponse de `GET
  /{domaine}/changes` et dans `GET /api/status`. S'il change (dossier de données
  déplacé/effacé → journal repartant de zéro), les clients le détectent et
  **réinitialisent leur curseur** au lieu de rater silencieusement les
  changements. Corrige le cas « une suppression n'est pas propagée aux autres
  postes après un déménagement des données du hub ».

## [0.2.4] — 2026-07-15
### Corrigé
- Le service systemd impose désormais `HOME` et `WorkingDirectory` (via
  `__RUN_HOME__`, injecté par `install-service.sh`) : un service SYSTEM avec
  `User=` ne reçoit pas toujours `$HOME`, ce qui empêchait le hub de résoudre son
  dossier de données (`%h` vaudrait `/root`). Défense en profondeur, en plus du
  garde-fou d'écriture et de la migration de config de la 0.2.3.

## [0.2.3] — 2026-07-15
### Corrigé
- **Plus de boucle de redémarrage** quand le dossier de données est
  inaccessible : au lieu d'une exception non catchée (crash → relance en
  boucle), le serveur vérifie que `dataDir` est créable et **inscriptible** au
  démarrage, et sort proprement avec un message clair sinon.
- `install-service.sh` **migre** une config héritée : un `dataDir` pointant vers
  `/var/lib/homeserverhub` (ancien service `DynamicUser`, inaccessible au compte
  utilisateur) est retiré automatiquement → emplacement par défaut dans le home.
  C'était la cause du service qui démarrait puis s'arrêtait après passage en
  `User=`.

## [0.2.2] — 2026-07-15
### Ajouté
- `scripts/linux/update-service.sh` : met à jour le binaire du service (avec
  `--build` optionnel), arrête/remplace/redémarre, sans toucher config ni
  données ; affiche la transition de version.

### Modifié
- `scripts/windows/install-service.ps1` fait aussi office de **mise à jour** :
  ré-exécuté, il arrête la tâche pour libérer l'exe, le remplace, puis redémarre.
- Windows : les données vont dans un dossier **accessible** (`dataDir` explicite
  sous `%ProgramData%\HomeServerHub\data`) au lieu du profil caché du compte
  SYSTEM.

## [0.2.1] — 2026-07-15
### Modifié
- **Dossier de données par défaut accessible à l'utilisateur.** Auparavant, le
  service systemd (`DynamicUser` + `StateDirectory`) écrivait dans
  `/var/lib/homeserverhub`, inaccessible à l'utilisateur. Désormais les données
  vont, par défaut, dans un emplacement standard **sous le home de l'utilisateur** :
  - Linux : `~/.local/share/morfredus/HomeServerHub` (XDG) ;
  - Windows : `%LOCALAPPDATA%\morfredus\HomeServerHub`.
  Le service systemd tourne maintenant **en tant que l'utilisateur** (`User=`),
  plus en `DynamicUser` ; `install-service.sh` injecte l'utilisateur courant et
  pré-crée le dossier. (`dataDir` explicite dans `config.json` reste prioritaire.)

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

[Non publié]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.7...HEAD
[0.2.7]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.6...v0.2.7
[0.2.6]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.5...v0.2.6
[0.2.5]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.4...v0.2.5
[0.2.4]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.3...v0.2.4
[0.2.3]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/morfredus/HomeServerHub_travail/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/morfredus/HomeServerHub_travail/releases/tag/v0.1.0
