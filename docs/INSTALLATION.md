# Installation et configuration de morfSync

Ce document explique comment **installer morfSync en service** (démarrage
automatique) sous **Linux** et **Windows**, et comment **configurer l'écoute
réseau** (adresse + port) sur les deux systèmes.

> Pour une prise en main pas à pas depuis zéro, voir plutôt
> [GUIDE_DEBUTANT.md](GUIDE_DEBUTANT.md). Le présent document est la référence.

---

## 0. Prérequis : compiler le binaire

L'installation suppose que `morfSync` (ou `morfSync.exe`) est déjà
compilé. Si ce n'est pas le cas, voir le [README](../README.md) :

```bash
# Linux / Raspberry Pi
cmake --preset linux   &&  cmake --build --preset linux      # -> build/morfSync

# Windows (MSYS2/MinGW)
cmake --preset mingw   &&  cmake --build --preset mingw      # -> build-mingw\morfSync.exe
```

---

## 1. Configuration de l'écoute réseau

Le hub lit un fichier **`config.json`**. Toutes les clés sont optionnelles : une
clé absente prend sa valeur par défaut. Le format est **identique sur Linux et
Windows** :

```json
{
  "host": "0.0.0.0",
  "port": 8080,
  "dataDir": "data",
  "token": ""
}
```

| Clé | Rôle | Défaut |
|-----|------|--------|
| `host` | Interface d'écoute. **`0.0.0.0` = accepte les connexions du réseau local** (autres postes). `127.0.0.1` = accessible uniquement depuis la machine elle-même. | `0.0.0.0` |
| `port` | Port TCP d'écoute. | `8080` |
| `dataDir` | Dossier où sont écrits les journaux de synchronisation (`{domaine}.json`). **Absent/vide = emplacement conforme à l'OS** (voir ci-dessous). | *(OS)* |
| `token` | Jeton partagé. **Vide = authentification désactivée** (réseau de confiance). Sinon, les clients doivent envoyer l'en-tête `Authorization: Bearer <token>`. | `""` |

### Où sont stockées les données par défaut

Si `dataDir` n'est pas précisé, morfSync choisit l'emplacement attendu par
le système :

| Système | Emplacement des données |
|---------|-------------------------|
| Linux | `~/.local/share/morfredus/morfSync` (ou `$XDG_DATA_HOME/morfredus/morfSync`) |
| Windows | `%LOCALAPPDATA%\morfredus\morfSync` (ou `%ProgramData%\morfredus\morfSync`) |

Ces emplacements sont **accessibles à l'utilisateur** (contrairement à
`/var/lib`). Le service systemd tourne donc **en tant que votre utilisateur**
(`User=`), pas en compte dynamique — `install-service.sh` s'en charge
automatiquement. Le dossier retenu est affiché dans le journal au démarrage, et
les données **survivent aux redémarrages** (le hub recharge les domaines
existants au lancement).

**Pour être joignable par un autre poste du réseau, `host` doit valoir `0.0.0.0`.**
C'est la valeur par défaut ; ne la mettez à `127.0.0.1` que pour un test purement local.

Le hub prend le chemin de la config en argument (sinon il cherche `config.json`
dans le dossier courant) :

```bash
./morfSync /chemin/vers/config.json
```

### Où vit la config une fois installée ?

| Système | Emplacement de `config.json` |
|---------|------------------------------|
| Linux (service) | `/etc/morfsync/config.json` |
| Windows (tâche)  | `C:\ProgramData\morfSync\config.json` |

Après modification de la config, **redémarrez** le service (voir plus bas).

---

## 2. Installation sous Linux (service systemd)

### Méthode automatique (recommandée)

Depuis la racine du dépôt, après avoir compilé :

```bash
sudo ./scripts/linux/install-service.sh
```

Le script : installe le binaire dans `/usr/local/bin`, crée
`/etc/morfsync/config.json` (s'il n'existe pas), installe le service,
l'active au démarrage, le lance, et teste `/api/health`.

Désinstallation :
```bash
sudo ./scripts/linux/install-service.sh --uninstall
```

### Méthode manuelle (équivalent, pour comprendre)

```bash
sudo install -m 0755 build/morfSync /usr/local/bin/morfSync
sudo mkdir -p /etc/morfsync
sudo cp config.example.json /etc/morfsync/config.json     # puis éditez-le
sudo cp scripts/linux/morfsync.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now morfsync                       # nom en minuscules
```

> ⚠️ Le nom de l'unité systemd est **`morfsync`** (tout en minuscules).
> `systemctl start MorfSync` (mauvaise casse) échouera (« Unit not found »).

Le service tourne **en tant que votre utilisateur** (`User=`) ; ses données vont
dans `~/.local/share/morfredus/morfSync` (voir §1). `install-service.sh` injecte
l'utilisateur et son home automatiquement.

### Piloter le service

```bash
systemctl status morfsync          # état
sudo systemctl restart morfsync    # après modification de config.json
sudo systemctl stop morfsync       # arrêter
journalctl -u morfsync -e          # journaux (diagnostic)
```

### Mettre à jour le binaire

Installer une nouvelle version **sans refaire l'installation** (config et
données conservées).

#### Méthode simple (un seul appel)

```bash
sudo ./scripts/linux/update-service.sh --build
```

`--build` fait tout : `git pull`, **reconstruction propre** (`rm -rf` du dossier
de build puis compilation, en tant que l'utilisateur), puis arrêt du service,
remplacement de `/usr/local/bin/morfSync` et redémarrage. Le script affiche
la version du nouveau binaire et la transition (ex. `0.2.5 -> 0.2.6`).

La mise à jour **complète aussi la configuration** : les valeurs déjà présentes
dans `/etc/morfsync/config.json` ne sont jamais modifiées, mais les paramètres
apparus depuis l'installation y sont ajoutés puis listés à l'écran. Sans cela,
un réglage introduit par une nouvelle version resterait absent indéfiniment et
la fonction correspondante ne s'activerait jamais, en silence. Une sauvegarde
est prise avant toute écriture ; `--no-config` laisse la configuration
strictement intacte. Si le fichier est absent, il est recopié depuis l'exemple.

L'**unité systemd** est également rafraîchie si le fichier `.service` du dépôt a
changé.

Le **preset est auto-détecté** selon l'architecture : `linux-arm64` sur un
Raspberry Pi 64 bits, `linux` sinon. Le forcer au besoin :

```bash
sudo ./scripts/linux/update-service.sh --build linux-arm64
```

#### Méthode manuelle (ordre exact, en cas de doute)

À suivre pas à pas si la mise à jour semble ne rien changer :

Sur **Raspberry Pi 64 bits**, utiliser le preset `linux-arm64` (dossier
`build-arm64`) ; sur un PC x86_64, `linux` (dossier `build`). Exemple pour le Pi :

```bash
cd ~/Codage/Apps/morfSync               # adapter au chemin du dépôt

git pull                                     # 1. récupérer le code
cat VERSION                                  # 2. confirmer la nouvelle version

rm -rf build-arm64                           # 3. build NEUF (étape indispensable)
cmake --preset linux-arm64
cmake --build --preset linux-arm64

./build-arm64/morfSync --version        # 4. vérifier la version compilée

sudo systemctl stop morfsync            # 5. remplacer le binaire du service
sudo install -m 0755 build-arm64/morfSync /usr/local/bin/morfSync
sudo systemctl restart morfsync

curl http://localhost:8080/api/health        # 6. vérifier la version en service
```

(Sur PC x86_64 : remplacer `linux-arm64` par `linux` et `build-arm64` par `build`.)

> **`rm -rf build` (étape 3) est indispensable.** Après un simple `git pull`,
> CMake ne recompile pas toujours : l'ancien binaire resterait dans `build/` et
> serait recopié tel quel (version inchangée). Un dossier `build/` neuf force la
> réintégration du numéro de version.
>
> **Vérifier la version** avec `morfSync --version` (fiable) plutôt qu'en
> cherchant une chaîne dans le binaire : « morfSync » et le numéro y sont
> stockés séparément.

### Pare-feu Linux

Si `ufw` est actif, autorisez le port :
```bash
sudo ufw allow 8080/tcp
```

---

## 3. Installation sous Windows (démarrage automatique)

morfSync est une application **console**. Windows n'a pas de « vrai service »
prêt à l'emploi pour ce type de programme sans outil externe (comme NSSM). On
utilise donc le **Planificateur de tâches** intégré : le hub démarre
automatiquement à l'allumage de la machine, en arrière-plan.

### Méthode automatique (recommandée)

1. Ouvrez **PowerShell en administrateur** (clic droit → « Exécuter en tant
   qu'administrateur »).
2. Depuis la racine du dépôt, après avoir compilé :

```powershell
.\scripts\windows\install-service.ps1
```

Le script : copie le binaire et la config dans `C:\ProgramData\morfSync`,
crée une tâche planifiée **`morfSync`** (démarrage automatique, compte
SYSTEM), **ouvre le port dans le pare-feu Windows**, démarre le hub et teste
`/api/health`.

Désinstallation :
```powershell
.\scripts\windows\install-service.ps1 -Uninstall
```

### Piloter la tâche

```powershell
Get-ScheduledTask -TaskName morfSync | Get-ScheduledTaskInfo   # état
Stop-ScheduledTask  -TaskName morfSync                          # arrêter
Start-ScheduledTask -TaskName morfSync                          # (re)démarrer
```
Après avoir modifié `C:\ProgramData\morfSync\config.json`, arrêtez puis
redémarrez la tâche.

### Pare-feu Windows

Le script d'installation ouvre automatiquement le port (profils Privé + Domaine).
Pour le faire manuellement :
```powershell
New-NetFirewallRule -DisplayName "morfSync" -Direction Inbound `
    -Action Allow -Protocol TCP -LocalPort 8080 -Profile Private,Domain
```

> Sans cette règle, `localhost:8080` répond mais **aucun autre poste ne peut
> joindre le hub** : c'est la cause n°1 d'échec de communication sur le réseau.

---

## 4. Vérifier que ça marche

Sur la machine du hub :
```bash
curl http://localhost:8080/api/health
# -> {"status":"ok","time":"...","version":"0.1.0"}
```

Depuis un autre poste du réseau (remplacez par l'IP de la machine du hub) :
```bash
curl http://192.168.1.50:8080/api/health
```
Trouver l'IP : `hostname -I` (Linux) ou `ipconfig` (Windows).

---

## 5. État actuel de la communication

Le hub expose déjà les endpoints de synchronisation (`/api/health`,
`GET`/`POST /api/{domaine}/changes`, voir [sync-contract.md](sync-contract.md)),
testables avec `curl` ou un navigateur.

**ComponentHub ne synchronise pas encore automatiquement avec le hub** : le
client de synchro (`SyncStorage` + boucle PUSH/PULL) est le prochain jalon
(voir [ROADMAP.md](../ROADMAP.md)). Pour l'instant, la « communication » se teste
donc à la main, pas depuis l'application.
