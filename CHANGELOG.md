# Changelog

Toutes les évolutions notables de morfSync sont consignées ici.
Format inspiré de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) ;
versionnage [SemVer](https://semver.org/lang/fr/).

## [0.7.0] - 2026-08-20

### Ajouté

- Mise à jour de la copie vendorée de morfDeploy 0.14.0 pour le packaging avec
  provenance vérifiée.

## [0.6.3] - 2026-08-18

### Ajouté

- **Déclaration de dépendance de build** dans `service.json` :
  `build_dependencies` = `nlohmann-json` (requis, `find_package(nlohmann_json)`).
  morfDeploy 0.9.0 le résout avant le build (Debian : `nlohmann-json3-dev` ; sur
  une toolchain sans gestionnaire, annonce le besoin). Aucun changement de code.

## [0.6.2] - 2026-08-14

### Corrigé

- Description de l'unité systemd : remplacement du tiret cadratin par un tiret
  simple, conformément à la règle de ponctuation du parc.

## [0.6.1] - 2026-08-14

### Modifié

- Ajout du marqueur de version manquant à la copie vendorée de **morfdeploy**
  (`third_party/morf/morfdeploy/VERSION` = 0.1.0), pour l'aligner sur le dépôt
  source `morfDeploy`. Le code Python vendoré était déjà à jour ; changement de
  forme uniquement.

## [0.6.0] - 2026-07-28

### Ajouté

- **Magasin de blobs adressé par contenu** pour transporter le binaire des pièces
  jointes entre postes sans alourdir le journal de changements (celui-ci ne garde
  que la référence de hash). Trois endpoints sous `/api/blob/{hash}` : `HEAD`
  (présence), `GET` (télécharger), `PUT` (téléverser, idempotent). Le hash est le
  SHA-256 du contenu ; le hub le traite comme une clé opaque et ne le recalcule
  pas (aucune dépendance crypto ajoutée), l'intégrité étant vérifiée par le client
  au téléchargement. Blobs rangés sous `{dataDir}/blobs/`, écriture atomique,
  plafond de garde-fou à 64 Mio. Voir `docs/sync-contract.md` §4.5 et
  `src/sync/blob_store.h`.

## [0.5.0] - 2026-07-28

### Modifié

- **Données déplacées sous `/var/lib/morfsystem/morfsync`** (doctrine du parc,
  voir `docs/FILESYSTEM.md`). Les journaux de synchro par domaine sont de
  l'**état persistant** généré par le service, distinct du programme (`/opt`) et
  de la config admin (`/etc`) : ils ne vivent plus sous `/opt/morfsync/data`.
  L'unité systemd déclare `StateDirectory=morfsystem/morfsync`, que systemd crée
  possédé par l'utilisateur du service et expose via `$STATE_DIRECTORY` ; le
  dossier de données par défaut suit cette variable. Plus besoin de provisionner
  les droits à la main. La clé `dataDir` de la configuration reste prioritaire
  pour un emplacement explicite.
  - **Migration** : une installation antérieure garde ses données sous
    `/opt/morfsync/data`. Les déplacer avant de démarrer cette version :
    `sudo systemctl stop morfsync && sudo mv /opt/morfsync/data/* /var/lib/morfsystem/morfsync/ 2>/dev/null; sudo systemctl start morfsync`
    (ou fixer `dataDir` sur l'ancien chemin dans la config).

## [0.4.0] - 2026-07-28

### Modifié

- **Configuration regroupée sous `/etc/morfsystem/<service>`.** Tout le parc
  partage désormais un point d'entrée UNIQUE dans `/etc` (`/etc/morfsystem/`),
  qui contient le fichier partagé `morfsystem.json` et un sous-dossier par
  service, au lieu d'un `/etc/<service>` par service à la racine de `/etc`. Sous
  Windows : `%ProgramData%\morfsystem\<service>`. Les données restent sous
  `/opt/<service>`. L'ancien `/etc/<service>` est adopté à l'installation
  (`migrate_from`).
- **Données rangées selon la convention morfSystem** (`docs/FILESYSTEM.md`).
  L'emplacement par défaut passe du dossier utilisateur
  (`~/.local/share/morfredus/morfSync`) à `<app_dir>/data`
  (`/opt/morfsync/data` sous Linux, `%ProgramData%\morfsync\data` sous Windows).
  morfSync est un service, pas une application : ses données sont sa source de
  vérité, personne ne les ouvre à la main, et toutes les opérations passent par
  les consommateurs via le réseau. Le réglage `dataDir` reste prioritaire ; les
  consommateurs ne voient aucune différence.

## [0.3.0] - 2026-07-22

### Ajouté

- **morfSync s'annonce sur le réseau (protocole morfbeacon/1) et sert `/status`.**
  Premier service du parc, il précédait le protocole que les autres respectent
  et restait invisible dans l'onglet Écosystème de morfMonitor. Il n'embarque
  pas morfBeacon (qui exige Qt) : c'est une implémentation sans dépendance,
  validée comme les autres par `check-protocol.py`. `/status` cohabite avec
  `/api/status`, que des clients existants appellent déjà.

### Modifié

- **La configuration revient dans `/etc/morfsync`**, à sa place selon la FHS -
  morfSync y était déjà avant d'être aligné par erreur sur les autres. Le
  déplacement est déclaré (`migrate_from`) : la config existante est adoptée,
  jamais écrasée. Le binaire va dans `/opt/morfsync`.
- **Installation, mise à jour et désinstallation par `./service.py`**, le point
  d'entrée unique multiplateforme (morfdeploy), en remplacement des scripts
  `install-service.sh`/`.ps1`. L'ancien binaire `/usr/local/bin/morfSync` est
  signalé, jamais supprimé.

- Documentation de version alignée sur les liens de dépôt de production.

## [0.2.9] - 2026-07-19

### Corrigé

- **La mise à jour ne livrait jamais les nouveaux paramètres de configuration.**
  `update-service.sh` ne recopiait que le binaire et laissait `/etc/morfsync/config.json`
  intact, par souci de préserver les réglages locaux. Conséquence : un paramètre
  introduit après l'installation restait absent indéfiniment, et la fonction
  correspondante ne s'activait jamais **sans que rien ne le signale**. La mise à
  jour **complète** désormais la configuration (`scripts/linux/merge-config.py`) :
  les valeurs déjà en place ne sont jamais modifiées, les clés manquantes sont
  ajoutées puis listées, et une sauvegarde précède toute écriture. Option
  `--no-config` pour laisser la configuration strictement intacte.
- **La configuration absente n'était pas recréée.** Après une installation
  partielle ou une suppression du dossier, la mise à jour laissait le service
  démarrer sans configuration. Elle est désormais recopiée depuis l'exemple.
- **L'unité systemd n'était pas rafraîchie.** Une modification du fichier
  `.service` dans le dépôt ne parvenait jamais à `/etc/systemd/system` : le
  service continuait de tourner avec l'ancienne définition.
- **L'analyse des options ne dépend plus de leur position.** Le script lisait
  `--build` en 1re position et son preset en 2e ; ajouter une option devant
  aurait cassé cette lecture.

> morfSync n'embarque aucune dépendance vendorée (`third_party/`) : contrairement
> aux autres services de l'écosystème, il n'y a rien à resynchroniser ici. Le
> sujet du commit précédent mentionne à tort une resynchronisation de morfBeacon.

## [0.2.8] - 2026-07-15
### Modifié - renommage HomeServerHub → morfSync

Le projet est renommé **morfSync** (famille *morf* : morfBeacon, morfUpdate…).
- **Produit / binaire / cible CMake** : `morfSync` (le binaire s'appelle désormais `morfSync`).
- **Service systemd** : `morfsync` (minuscules) ; config dans **`/etc/morfsync`**
  (`install-service.sh` migre automatiquement depuis `/etc/homeserverhub`).
- **Dossier de données** : `~/.local/share/morfredus/morfSync` (Linux) /
  `%LOCALAPPDATA%\morfredus\morfSync` (Windows).
- **Tâche + dossier Windows** : `morfsync` / `C:\ProgramData\morfsync`.
- **Dépôt GitHub** : `HomeServerHub` → `morfSync`.

> Migration : désinstaller l'ancien service `homeserverhub` (voir notes), puis
> relancer `install-service.sh` (la config est récupérée automatiquement). Le
> dossier de données change de nom : la détection d'époque (0.2.5) fait que les
> clients se re-synchronisent tout seuls.

## [0.2.7] - 2026-07-15
### Corrigé
- `update-service.sh --build` **choisit le bon preset selon l'architecture** :
  `linux-arm64` (dossier `build-arm64`) sur un Raspberry Pi 64 bits, `linux`
  sinon. Auparavant le preset `linux` était codé en dur. Le preset peut aussi
  être forcé en argument (`--build linux-arm64-cross`). Documentation de mise à
  jour ajustée en conséquence.

## [0.2.6] - 2026-07-15
### Ajouté
- Option **`--version` / `-v`** : affiche la version sans démarrer le serveur
  (vérification fiable - le nom et le numéro sont stockés séparément dans le
  binaire, donc introuvables par un simple `grep`).

### Corrigé
- `update-service.sh --build` fait désormais **`git pull` puis une
  reconstruction propre** (`rm -rf build`) : après un simple `git pull`, CMake ne
  recompilait pas toujours et l'ancien binaire (version périmée) était recopié.
  La mise à jour du service ne remplaçait alors rien.
- Documentation : procédure de mise à jour **complète et fiable** dans
  `docs/INSTALLATION.md` (build neuf + vérification via `--version`).

## [0.2.5] - 2026-07-15
### Ajouté
- **Identité de journal (« époque ») par domaine** : un `journalId` stable est
  généré à la création d'un journal et exposé dans la réponse de `GET
  /{domaine}/changes` et dans `GET /api/status`. S'il change (dossier de données
  déplacé/effacé → journal repartant de zéro), les clients le détectent et
  **réinitialisent leur curseur** au lieu de rater silencieusement les
  changements. Corrige le cas « une suppression n'est pas propagée aux autres
  postes après un déménagement des données du hub ».

## [0.2.4] - 2026-07-15
### Corrigé
- Le service systemd impose désormais `HOME` et `WorkingDirectory` (via
  `__RUN_HOME__`, injecté par `install-service.sh`) : un service SYSTEM avec
  `User=` ne reçoit pas toujours `$HOME`, ce qui empêchait le hub de résoudre son
  dossier de données (`%h` vaudrait `/root`). Défense en profondeur, en plus du
  garde-fou d'écriture et de la migration de config de la 0.2.3.

## [0.2.3] - 2026-07-15
### Corrigé
- **Plus de boucle de redémarrage** quand le dossier de données est
  inaccessible : au lieu d'une exception non catchée (crash → relance en
  boucle), le serveur vérifie que `dataDir` est créable et **inscriptible** au
  démarrage, et sort proprement avec un message clair sinon.
- `install-service.sh` **migre** une config héritée : un `dataDir` pointant vers
  `/var/lib/morfsync` (ancien service `DynamicUser`, inaccessible au compte
  utilisateur) est retiré automatiquement → emplacement par défaut dans le home.
  C'était la cause du service qui démarrait puis s'arrêtait après passage en
  `User=`.

## [0.2.2] - 2026-07-15
### Ajouté
- `scripts/linux/update-service.sh` : met à jour le binaire du service (avec
  `--build` optionnel), arrête/remplace/redémarre, sans toucher config ni
  données ; affiche la transition de version.

### Modifié
- `scripts/windows/install-service.ps1` fait aussi office de **mise à jour** :
  ré-exécuté, il arrête la tâche pour libérer l'exe, le remplace, puis redémarre.
- Windows : les données vont dans un dossier **accessible** (`dataDir` explicite
  sous `%ProgramData%\morfSync\data`) au lieu du profil caché du compte
  SYSTEM.

## [0.2.1] - 2026-07-15
### Modifié
- **Dossier de données par défaut accessible à l'utilisateur.** Auparavant, le
  service systemd (`DynamicUser` + `StateDirectory`) écrivait dans
  `/var/lib/morfsync`, inaccessible à l'utilisateur. Désormais les données
  vont, par défaut, dans un emplacement standard **sous le home de l'utilisateur** :
  - Linux : `~/.local/share/morfredus/morfSync` (XDG) ;
  - Windows : `%LOCALAPPDATA%\morfredus\morfSync`.
  Le service systemd tourne maintenant **en tant que l'utilisateur** (`User=`),
  plus en `DynamicUser` ; `install-service.sh` injecte l'utilisateur courant et
  pré-crée le dossier. (`dataDir` explicite dans `config.json` reste prioritaire.)

## [0.2.0] - 2026-07-15
### Ajouté
- Stockage des données à l'emplacement conforme à l'OS quand `dataDir` n'est pas
  précisé : `$STATE_DIRECTORY` (service systemd) puis XDG sous Linux,
  `%ProgramData%\morfSync` sous Windows (`src/app/paths.*`).
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
  plus récente - résolution déterministe *highest-rev puis updatedAt* (au lieu du
  last-write-wins par arrivée). Permet au client de re-pousser sans générer de
  bruit ; combiné au PUSH incrémental côté ComponentHub, la synchro passe à
  l'échelle.

## [0.1.0] - 2026-07-15
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
- Test de fumée headless du journal (option `MS_BUILD_SMOKE`).
- Contrat de synchronisation versionné dans `docs/sync-contract.md`.
- Service systemd et README bilingue (EN/FR).

[0.4.0]: https://github.com/morfredus/morfSync/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/morfredus/morfSync/compare/v0.2.8...v0.3.0
[0.2.8]: https://github.com/morfredus/morfSync/compare/v0.2.7...v0.2.8
[0.2.7]: https://github.com/morfredus/morfSync/compare/v0.2.6...v0.2.7
[0.2.6]: https://github.com/morfredus/morfSync/compare/v0.2.5...v0.2.6
[0.2.5]: https://github.com/morfredus/morfSync/compare/v0.2.4...v0.2.5
[0.2.4]: https://github.com/morfredus/morfSync/compare/v0.2.3...v0.2.4
[0.2.3]: https://github.com/morfredus/morfSync/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/morfredus/morfSync/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/morfredus/morfSync/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/morfredus/morfSync/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/morfredus/morfSync/releases/tag/v0.1.0
