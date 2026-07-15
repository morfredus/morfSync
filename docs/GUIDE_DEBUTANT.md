# HomeServerHub — Guide du débutant

Ce guide vous prend **par la main, de zéro**, pour faire fonctionner
HomeServerHub et vérifier qu'une machine peut communiquer avec lui sur votre
réseau. Aucune connaissance préalable n'est supposée. Suivez les étapes dans
l'ordre.

---

## C'est quoi, HomeServerHub ?

C'est un **petit serveur** qui tourne en permanence sur une machine de votre
maison (un Raspberry Pi, un PC toujours allumé…). Son rôle : servir de **point
de rendez-vous** pour synchroniser les données de vos applications entre vos
différents postes.

Trois idées à retenir :
- Vos applications **fonctionnent sans lui** (elles ont leur copie locale).
- Le hub ne fait que **recevoir et redistribuer** des changements.
- Vous choisissez **une seule** machine qui héberge le hub ; les autres s'y
  connectent.

> **Important, à lire une fois :** aujourd'hui, ComponentHub ne se connecte pas
> *encore* automatiquement au hub (c'est le prochain chantier). Ce guide vous
> montre donc comment lancer le hub et **tester la communication à la main** —
> ce qui prouve que toute la plomberie réseau fonctionne.

---

## Vous aurez besoin de

**Matériel**
- **Une machine « hub »** qui reste allumée (idéalement un **Raspberry Pi**).
- **Un autre poste** (Windows, portable…) qui servira de client de test.
- Les deux **sur le même réseau local** (même box/routeur, en Wi-Fi ou câble).

**Logiciels sur la machine hub** (pour compiler — une seule fois)
- Un compilateur **C++17**, **CMake** (≥ 3.21) et **Ninja**.
- La bibliothèque **nlohmann-json** (seule dépendance externe).
- **git**, pour récupérer le dépôt.

**Pour tester** (envoyer des requêtes au hub)
- **`curl`** — présent d'origine sur Linux/Raspberry, et sur Windows 10/11.
  C'est la méthode recommandée, surtout sur un **Raspberry sans écran ni
  navigateur**.
- Un **navigateur web** est pratique *si* la machine en a un (poste de bureau),
  mais **pas nécessaire** : tout se vérifie au terminal avec `curl`.

**Une information à noter**
- L'**adresse IP** de la machine hub sur le réseau (voir Étape 1).

---

## Étape 1 — Choisir la machine « hub »

Choisissez **une** machine qui restera allumée : idéalement le **Raspberry Pi**.
Les autres postes (Windows, portable…) seront des **clients** qui la contactent.

Notez, on en aura besoin :
- **Le système** de la machine hub : Linux (Raspberry) ou Windows.
- **Son adresse IP** sur le réseau. Pour la trouver :
  - Linux : tapez `hostname -I` → prenez le 1ᵉʳ nombre, ex. `192.168.1.50`.
  - Windows : tapez `ipconfig` → ligne « Adresse IPv4 », ex. `192.168.1.42`.

---

## Étape 2 — Récupérer et compiler HomeServerHub

Sur la machine hub, ouvrez un terminal et placez-vous dans le dépôt.

**Sur Raspberry Pi / Linux :**
```bash
sudo apt install nlohmann-json3-dev cmake ninja-build   # une seule fois
cmake --preset linux
cmake --build --preset linux
```
Résultat : un fichier `build/HomeServerHub`.

**Sur Windows (avec MSYS2/MinGW installé) :**
```bash
cmake --preset mingw
cmake --build --preset mingw
```
Résultat : un fichier `build-mingw\HomeServerHub.exe`.

> Si la compilation se plaint de `nlohmann_json` introuvable, c'est la seule
> dépendance à installer (paquet ci-dessus sous Linux ; `pacman -S
> mingw-w64-x86_64-nlohmann-json` sous MSYS2).

---

## Étape 3 — Premier lancement (test rapide, sans installation)

Avant d'installer quoi que ce soit, lançons le hub « à la main » pour voir
qu'il démarre.

**Linux :**
```bash
cd build
./HomeServerHub
```
**Windows :**
```powershell
cd build-mingw
.\HomeServerHub.exe
```

Vous devez voir une ligne comme :
```
HomeServerHub 0.1.0 — écoute sur http://0.0.0.0:8080 (données: data, auth: désactivée)
```
🎉 Le hub tourne. **Laissez ce terminal ouvert** et ouvrez-en un **second** pour
la suite.

**Vérification — au terminal (fonctionne partout, même sur un Raspberry sans
écran) :** dans le second terminal, tapez :
```bash
curl http://localhost:8080/api/health
```
Vous devez voir : `{"status":"ok", ...}`. Si oui, tout va bien.

> *Alternative si la machine a un navigateur* (poste de bureau) : ouvrez
> `http://localhost:8080/api/health`. Sur un Raspberry « headless » (sans
> écran), restez sur `curl`.
>
> *Pas de `curl` ?* Sous Linux : `sudo apt install curl`. Sinon, en pur
> terminal, `wget -qO- http://localhost:8080/api/health` fait la même chose.

Pour arrêter le hub dans ce mode manuel : revenez au premier terminal et faites
**Ctrl + C**.

---

## Étape 4 — L'installer pour qu'il démarre tout seul

Une fois le test OK, installez-le en démarrage automatique pour ne plus avoir à
le lancer à la main.

**Sur Raspberry Pi / Linux :**
```bash
sudo ./scripts/linux/install-service.sh
```

**Sur Windows** — ouvrez **PowerShell en administrateur** (clic droit →
« Exécuter en tant qu'administrateur »), puis :
```powershell
.\scripts\windows\install-service.ps1
```

Les scripts font tout : copie des fichiers, démarrage automatique, ouverture du
pare-feu, et un test final. Détails et désinstallation dans
[INSTALLATION.md](INSTALLATION.md).

---

## Étape 5 — Le test qui compte : communiquer depuis un AUTRE poste

C'est ici qu'on prouve que le réseau fonctionne. On va, **depuis un second
poste**, parler au hub qui tourne sur la machine choisie à l'étape 1.

Remplacez `192.168.1.50` par l'**IP de la machine hub** (étape 1).

**5.a — Le hub répond-il depuis l'autre poste ?**
```bash
curl http://192.168.1.50:8080/api/health
```
Réponse attendue : `{"status":"ok", ...}`.
Si ça bloque → voir la section « Ça ne marche pas » plus bas (souvent le pare-feu).

**5.b — Envoyer une donnée (PUSH), puis la relire (PULL).**
On simule un composant envoyé par ce poste :
```bash
curl -X POST http://192.168.1.50:8080/api/componenthub/changes \
  -H "Content-Type: application/json" \
  -d '{"deviceId":"mon-portable","changes":[
        {"id":"11111111-1111-4111-8111-111111111111","type":"component",
         "rev":1,"deleted":false,"data":{"name":"Test depuis le portable"}}]}'
```
Réponse attendue : `{"applied":[{"id":"1111...","seq":1}], ... }`.

Maintenant, relisez ce que le hub connaît :
```bash
curl "http://192.168.1.50:8080/api/componenthub/changes?since=0"
```
Vous devez retrouver votre composant « Test depuis le portable ».
**Bravo — la communication fonctionne de bout en bout entre deux machines.** ✅

---

## Étape 6 — Régler le port ou ajouter un mot de passe (facultatif)

Ouvrez le fichier `config.json` :
- Linux : `/etc/homeserverhub/config.json`
- Windows : `C:\ProgramData\HomeServerHub\config.json`

Changez par exemple le port, ou ajoutez un jeton :
```json
{ "host": "0.0.0.0", "port": 8080, "token": "un-mot-de-passe-partage" }
```
Puis **redémarrez** :
- Linux : `sudo systemctl restart homeserverhub`
- Windows : `Stop-ScheduledTask -TaskName HomeServerHub ; Start-ScheduledTask -TaskName HomeServerHub`

Si vous mettez un `token`, les clients devront ajouter l'en-tête
`-H "Authorization: Bearer un-mot-de-passe-partage"` à leurs requêtes (sauf
`/api/health` qui reste ouvert).

---

## « Ça ne marche pas » — les cas les plus fréquents

| Symptôme | Cause probable | Solution |
|----------|----------------|----------|
| `Unit HomeServerhub.service not found` (Linux) | Mauvaise casse ou service non installé | Le nom est **`homeserverhub`** (minuscules). Relancez `sudo ./scripts/linux/install-service.sh`. |
| `localhost:8080` marche, mais **pas** depuis un autre poste | Pare-feu qui bloque le port | Windows : relancez le script en admin (il ouvre le pare-feu). Linux : `sudo ufw allow 8080/tcp`. |
| Un autre poste ne répond pas | Mauvaise IP, ou `host` mis à `127.0.0.1` | Vérifiez l'IP (`hostname -I` / `ipconfig`) et que `config.json` a `"host": "0.0.0.0"`. |
| `impossible d'ouvrir le port 8080 (déjà utilisé ?)` | Un autre programme (ou un 2ᵉ hub) occupe le port | Changez le `port` dans `config.json`, ou fermez l'autre programme. |
| Rien ne s'affiche / démarrage silencieux | En mode service, la sortie va dans les journaux | Linux : `journalctl -u homeserverhub -e`. Windows : `Get-ScheduledTask -TaskName HomeServerHub | Get-ScheduledTaskInfo`. |
| La compilation échoue sur `nlohmann_json` | Dépendance manquante | Linux : `sudo apt install nlohmann-json3-dev`. MSYS2 : `pacman -S mingw-w64-x86_64-nlohmann-json`. |

---

## Et après ?

Le hub est en place et communique. La prochaine étape du projet est de **brancher
ComponentHub dessus** pour que la synchronisation se fasse toute seule, sans
`curl`. Ce sera décrit ici une fois disponible (voir [ROADMAP.md](../ROADMAP.md)).
