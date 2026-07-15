# Installation et configuration de HomeServerHub

Ce document explique comment **installer HomeServerHub en service** (démarrage
automatique) sous **Linux** et **Windows**, et comment **configurer l'écoute
réseau** (adresse + port) sur les deux systèmes.

> Pour une prise en main pas à pas depuis zéro, voir plutôt
> [GUIDE_DEBUTANT.md](GUIDE_DEBUTANT.md). Le présent document est la référence.

---

## 0. Prérequis : compiler le binaire

L'installation suppose que `HomeServerHub` (ou `HomeServerHub.exe`) est déjà
compilé. Si ce n'est pas le cas, voir le [README](../README.md) :

```bash
# Linux / Raspberry Pi
cmake --preset linux   &&  cmake --build --preset linux      # -> build/HomeServerHub

# Windows (MSYS2/MinGW)
cmake --preset mingw   &&  cmake --build --preset mingw      # -> build-mingw\HomeServerHub.exe
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

Si `dataDir` n'est pas précisé, HomeServerHub choisit l'emplacement attendu par
le système :

| Système | Emplacement des données |
|---------|-------------------------|
| Linux (service systemd) | `/var/lib/homeserverhub` (via `StateDirectory`, variable `$STATE_DIRECTORY`) |
| Linux (lancement manuel) | `$XDG_DATA_HOME/homeserverhub` ou `~/.local/share/homeserverhub` |
| Windows | `%ProgramData%\HomeServerHub` (ou `%LOCALAPPDATA%\HomeServerHub`) |

Le dossier retenu est affiché dans le journal au démarrage. Les données
**survivent aux redémarrages** : au lancement, le hub recharge les domaines déjà
présents.

**Pour être joignable par un autre poste du réseau, `host` doit valoir `0.0.0.0`.**
C'est la valeur par défaut ; ne la mettez à `127.0.0.1` que pour un test purement local.

Le hub prend le chemin de la config en argument (sinon il cherche `config.json`
dans le dossier courant) :

```bash
./HomeServerHub /chemin/vers/config.json
```

### Où vit la config une fois installée ?

| Système | Emplacement de `config.json` |
|---------|------------------------------|
| Linux (service) | `/etc/homeserverhub/config.json` |
| Windows (tâche)  | `C:\ProgramData\HomeServerHub\config.json` |

Après modification de la config, **redémarrez** le service (voir plus bas).

---

## 2. Installation sous Linux (service systemd)

### Méthode automatique (recommandée)

Depuis la racine du dépôt, après avoir compilé :

```bash
sudo ./scripts/linux/install-service.sh
```

Le script : installe le binaire dans `/usr/local/bin`, crée
`/etc/homeserverhub/config.json` (s'il n'existe pas), installe le service,
l'active au démarrage, le lance, et teste `/api/health`.

Désinstallation :
```bash
sudo ./scripts/linux/install-service.sh --uninstall
```

### Méthode manuelle (équivalent, pour comprendre)

```bash
sudo install -m 0755 build/HomeServerHub /usr/local/bin/HomeServerHub
sudo mkdir -p /etc/homeserverhub
sudo cp config.example.json /etc/homeserverhub/config.json     # puis éditez-le
sudo cp scripts/linux/homeserverhub.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now homeserverhub                       # nom en minuscules
```

> ⚠️ Le nom de l'unité systemd est **`homeserverhub`** (tout en minuscules).
> `systemctl start HomeServerhub` échouera (« Unit not found »).

Le service tourne sous un utilisateur dédié (`DynamicUser`) ; ses données vont
dans `/var/lib/homeserverhub/`. Réglez donc `"dataDir": "/var/lib/homeserverhub/data"`
dans la config (le script d'installation le fait automatiquement).

### Piloter le service

```bash
systemctl status homeserverhub          # état
sudo systemctl restart homeserverhub    # après modification de config.json
sudo systemctl stop homeserverhub       # arrêter
journalctl -u homeserverhub -e          # journaux (diagnostic)
```

### Pare-feu Linux

Si `ufw` est actif, autorisez le port :
```bash
sudo ufw allow 8080/tcp
```

---

## 3. Installation sous Windows (démarrage automatique)

HomeServerHub est une application **console**. Windows n'a pas de « vrai service »
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

Le script : copie le binaire et la config dans `C:\ProgramData\HomeServerHub`,
crée une tâche planifiée **`HomeServerHub`** (démarrage automatique, compte
SYSTEM), **ouvre le port dans le pare-feu Windows**, démarre le hub et teste
`/api/health`.

Désinstallation :
```powershell
.\scripts\windows\install-service.ps1 -Uninstall
```

### Piloter la tâche

```powershell
Get-ScheduledTask -TaskName HomeServerHub | Get-ScheduledTaskInfo   # état
Stop-ScheduledTask  -TaskName HomeServerHub                          # arrêter
Start-ScheduledTask -TaskName HomeServerHub                          # (re)démarrer
```
Après avoir modifié `C:\ProgramData\HomeServerHub\config.json`, arrêtez puis
redémarrez la tâche.

### Pare-feu Windows

Le script d'installation ouvre automatiquement le port (profils Privé + Domaine).
Pour le faire manuellement :
```powershell
New-NetFirewallRule -DisplayName "HomeServerHub" -Direction Inbound `
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
