# HomeServerHub — Contrat de synchronisation (v1)

> Document de référence de l'écosystème. Tous les projets clients (ComponentHub,
> MeteoHub, RaspberryDashboard, SiteWatch, morfBeacon, morfUpdate…) suivent ce
> contrat. Il définit **le modèle de données synchronisable**, **le protocole
> REST**, et **l'interface de stockage** — rien d'autre. Le reste de chaque
> application ignore totalement comment la synchro fonctionne.

Statut : **v1 — draft à valider**. Philosophie : *offline-first*, simple,
last-write-wins arbitré par le serveur. On ne complexifie pas tant que ce socle
ne fait pas mal.

---

## 0. Principes non négociables

1. **Le client travaille toujours sur sa copie locale.** Le réseau est un confort, jamais une dépendance.
2. **Le serveur ordonne, le client n'arbitre pas.** Aucune comparaison d'horloge murale entre appareils (voir §3 : ESP32 dérive, le Raspberry Pi n'a pas de RTC).
3. **On ne supprime pas physiquement.** On marque `deleted=true` (tombstone). Le nettoyage réel est une opération de maintenance séparée et tardive.
4. **Identité permanente = UUID.** Jamais le nom, jamais un entier auto-incrémenté.

---

## 1. L'enveloppe de synchronisation

Chaque **entité synchronisable** (un composant, une localisation, un projet, une
mesure météo…) porte le même bloc de métadonnées. C'est le seul contrat commun.

```jsonc
{
  // --- enveloppe (universelle, gérée par la couche sync) ---
  "id":         "550e8400-e29b-41d4-a716-446655440000", // UUID v4, permanent
  "type":       "component",        // discriminant d'entité
  "createdAt":  "2026-07-14T10:12:00Z", // ISO 8601 UTC — affichage seulement
  "updatedAt":  "2026-07-14T11:47:33Z", // ISO 8601 UTC — affichage seulement
  "deleted":    false,              // tombstone
  "rev":        7,                  // compteur de révision local, incrémenté à chaque save
  "origin":     "windows-fred",     // deviceId ayant produit cette révision

  // --- charge utile métier (spécifique au type) ---
  "data": {
    "kind": "component",
    "name": "BME280",
    "reference": "...",
    "quantity": 3
    // ... tous les champs métier de l'entité
  }
}
```

**Règles :**

- `id`, `createdAt`, `deleted`, `data` : gérés par le client.
- `updatedAt` et `rev` : incrémentés par le client **à chaque `save()`** (`rev += 1`).
- `updatedAt`/`createdAt` servent à **l'affichage humain uniquement**, jamais à l'arbitrage.
- `origin` = identifiant stable de l'appareil (voir §5), utile pour le débogage et les stats.

> ⚠️ **`updatedAt` n'arbitre rien.** C'est le numéro de séquence serveur (§2) qui
> ordonne. On garde quand même `updatedAt` parce qu'il est lisible et « suffisamment
> juste » pour l'UI, mais on ne fonde jamais une décision de conflit dessus.

---

## 2. Le journal de changements (cœur du protocole)

Le serveur ne « pense » pas la logique métier. Il tient **un unique journal
ordonné** par domaine (ou global — au choix, commencer global). Chaque changement
reçu se voit attribuer un **numéro de séquence monotone `seq`** (entier, jamais
réutilisé, jamais remis à zéro).

```
seq  | id (uuid)   | type       | deleted | rev | origin        | payload
-----+-------------+------------+---------+-----+---------------+---------
1041 | 550e8400... | component  | false   | 7   | windows-fred  | {...}
1042 | 7a3f...     | location   | false   | 2   | mac-fred      | {...}
1043 | 550e8400... | component  | true    | 8   | mac-fred      | {...}   ← tombstone
```

Le client mémorise **un seul curseur** : `lastSeq` = le plus grand `seq` qu'il a
déjà intégré. Toute la synchro se résume à :

1. **PULL** : « donne-moi tout ce qui a `seq > lastSeq` ».
2. **PUSH** : « voici mes changements locaux ; assigne-leur des `seq` ».

Pas de calcul de diff côté client. Pas de comparaison d'horloge. Le serveur est
la source de vérité de l'**ordre**.

---

## 3. Résolution des conflits (v1 : simple et déterministe)

Deux appareils modifient la même entité `id` hors ligne, puis se synchronisent.

**Règle v1 — Last-Write-Wins arbitré par le serveur :**

> Quand le serveur reçoit un PUSH pour une entité `id` qui existe déjà, la
> version poussée **remplace** l'état courant et reçoit un nouveau `seq`
> (le plus grand). Le dernier à atteindre le serveur gagne.

- Simple, déterministe, aucun état de conflit à gérer dans l'UI.
- Un tombstone (`deleted=true`) est un changement comme un autre : il peut être
  « annulé » par une modification ultérieure (undelete), et inversement.

**Garde-fou anti-perte silencieuse (recommandé dès v1) :** le serveur peut
renvoyer, dans la réponse au PUSH, la liste des `id` où **le `rev` poussé était
inférieur au `rev` déjà connu** (= « tu écrasais une version plus récente que la
tienne »). Le client les journalise. Ça ne bloque pas la synchro, mais ça rend
les écrasements visibles au lieu de les cacher.

**Évolutions futures (hors v1, ne pas coder maintenant) :** choix utilisateur,
conservation des deux versions, merge champ par champ. L'enveloppe (`rev` +
`origin`) suffit déjà à les implémenter plus tard sans casser le format.

---

## 4. API REST

Base : `http://homeserverhub.local:8080` (IP manuelle en secours).
Auth : en-tête `Authorization: Bearer <token>` (voir §7).
Tous les corps sont en JSON UTF-8.

### 4.1 État du serveur
```
GET /api/health
→ 200 { "status": "ok", "time": "2026-07-14T12:00:00Z", "version": "1.0.0" }
```
Utilisé par « tester la connexion » et « voir l'état du serveur » dans les préférences client.

### 4.2 PULL — récupérer les changements
```
GET /api/{domain}/changes?since={lastSeq}&limit={n}
→ 200 {
    "changes": [ { "seq": 1042, "id": "...", "type": "location",
                   "deleted": false, "rev": 2, "origin": "mac-fred",
                   "createdAt": "...", "updatedAt": "...", "data": {...} },
                 ... ],
    "lastSeq": 1043,     // plus grand seq de ce lot
    "hasMore": false     // true si limité → refaire un PULL avec since=lastSeq
  }
```
- `{domain}` : `componenthub`, `meteohub`, … (un journal par projet). Commencer par un seul suffit.
- `since=0` → réplication initiale complète (tombstones compris).
- Pagination via `limit` + `hasMore` : indispensable pour l'ESP32 et le premier sync.

### 4.3 PUSH — envoyer ses changements locaux
```
POST /api/{domain}/changes
body { "deviceId": "windows-fred",
       "changes": [ { "id": "...", "type": "component", "deleted": false,
                      "rev": 8, "createdAt": "...", "updatedAt": "...",
                      "data": {...} }, ... ] }
→ 200 {
    "applied":  [ { "id": "...", "seq": 1044 }, ... ],  // seq attribué à chaque changement
    "conflicts": [ { "id": "...", "serverRev": 9, "yourRev": 8 } ], // écrasements détectés (§3)
    "lastSeq": 1045
  }
```
- Le client envoie **sans `seq`** (il ne le connaît pas encore) ; le serveur le renvoie dans `applied`.
- **Idempotence** : rejouer un PUSH avec les mêmes `(id, rev)` ne crée pas de doublon (le serveur reconnaît `id` et compare `rev`). Un PUSH interrompu peut être rejoué sans risque.

### 4.4 Ordre d'une session de synchro
```
1. GET /api/health              → serveur joignable ?
2. PUSH mes changements locaux  → j'envoie d'abord ce que j'ai fait hors ligne
3. PULL depuis lastSeq          → je récupère le reste (dont mes propres changements ré-ordonnés)
4. Applique le PULL localement, mets à jour lastSeq
```
> On PUSH avant de PULL : ainsi le PULL renvoie un état déjà cohérent incluant nos propres écritures avec leur `seq` définitif.

---

## 5. Identité de l'appareil et curseur

Chaque installation stocke, à côté de sa base locale, un petit fichier d'état de
synchro (jamais synchronisé, propre à la machine) :

```jsonc
// sync_state.json  (local, non synchronisé)
{
  "deviceId": "windows-fred",   // UUID ou slug stable, généré au 1er lancement
  "serverUrl": "http://homeserverhub.local:8080",
  "token": "…",
  "cursors": { "componenthub": 1043, "meteohub": 88 } // lastSeq par domaine
}
```

`deviceId` : généré une fois, persistant. Sert à `origin` et au débogage.

---

## 6. L'interface de stockage `IStorage`

Le reste de l'application ne connaît **que** cette interface. Elle a plusieurs
implémentations interchangeables. Pour ComponentHub, elle se pose **au-dessus**
du pattern `IComponentRepository` existant, sans le jeter.

### 6.1 Concept (neutre)
```
interface IStorage<T> {
    findAll()            -> list<T>        // hors tombstones par défaut
    findById(id: Uuid)   -> T?
    save(entity: T)      -> T              // upsert : rev += 1, updatedAt = now
    remove(id: Uuid)     -> bool           // tombstone : deleted = true (PAS de delete physique)
    // --- réservé à la couche sync ---
    findChangedSince(...)-> list<T>        // pour construire un PUSH
    applyRemote(change)  -> void           // pour intégrer un PULL (LWW)
}
```

### 6.2 Implémentations prévues
- **`JsonStorage`** — la base locale (aujourd'hui). Aucune dépendance réseau.
- **`SyncStorage`** — décorateur : délègue à `JsonStorage` pour tout, et déclenche/gère la synchro réseau. L'app ne voit qu'un `IStorage`.
- **`SqliteStorage`** (plus tard) — remplace `JsonStorage` sans toucher au reste.

### 6.3 Esquisse C++ pour ComponentHub (aligne sur l'existant)

`Component` gagne l'enveloppe (voir `src/domain/component.h`) :

```cpp
// AVANT : using Id = int;  → À MIGRER
using Uuid = std::string;   // UUID v4

struct SyncMeta {
    Uuid        id;
    std::string createdAt;   // ISO 8601 UTC
    std::string updatedAt;   // ISO 8601 UTC
    bool        deleted = false;
    int         rev = 0;
    std::string origin;      // deviceId
};

struct Component {
    SyncMeta meta;           // ← l'enveloppe
    ComponentKind kind = ComponentKind::Component;
    std::string name;
    // ... champs métier inchangés
};
```

Ton `IComponentRepository` (`findAll/findById/save/remove/saveAll`) devient une
implémentation concrète de `IStorage<Component>`. `remove()` change de sémantique :
il pose `deleted=true` au lieu d'effacer. `save()` incrémente `rev` et `updatedAt`.

### 6.4 Boucle de synchro (pseudocode, côté client)
```
function sync(domain):
    if not GET /health: return            # offline → on ne fait rien, l'app continue
    local  = storage.findChangedSince(cursor.pushWatermark)
    resp   = POST /{domain}/changes { deviceId, changes: local }
    for c in resp.conflicts: log("écrasé:", c.id)   # visibilité, pas de blocage
    pull   = GET /{domain}/changes?since=cursor.lastSeq
    for change in pull.changes:
        storage.applyRemote(change)        # LWW : la version distante remplace la locale
    cursor.lastSeq = pull.lastSeq
    while pull.hasMore: repeat PULL
```

---

## 7. Authentification (v1 : minimale, LAN privé)

Réseau local privé → pas de sur-ingénierie.
- Un **token partagé statique** (`Bearer`) configuré côté serveur et dans chaque client suffit pour v1.
- Pas de comptes, pas d'OAuth, pas de sessions tant que le besoin n'existe pas.
- `HomeServerHub` pourra fournir plus tard une vraie authentification comme *service commun* — sans changer ce contrat de synchro.

---

## 8. Cas particulier : appareils embarqués (ESP32 — MeteoHub, morfBeacon)

Le **contrat** (enveloppe + REST) reste identique. L'**implémentation** de
`IStorage` diffère : un ESP32 ne réplique pas une base complète.

- **PUSH seulement** pour ses données produites (mesures météo) : il envoie, il ne pull pas l'inventaire.
- **PULL ciblé** pour sa configuration uniquement (`GET /meteohub/config` ou un `type=config`).
- Curseur et `deviceId` stockés en NVS/SPIFFS.
- Pagination (`limit`) obligatoire : mémoire contrainte.

C'est cohérent avec ton idée « une seule architecture » : c'est le **même
contrat**, avec un profil client léger.

---

## 9. Migration ComponentHub (entiers → UUID)

Le seul vrai chantier de bascule. Une passe unique, une fois :
1. Pour chaque entité existante : générer un `id` UUID v4, initialiser `createdAt=updatedAt=now`, `rev=1`, `deleted=false`, `origin=deviceId`.
2. **Remapper les références internes** (`locationId`, `projectId`, liens projet-composant) des anciens `int` vers les nouveaux UUID via une table de correspondance `oldInt → uuid`.
3. Écrire la nouvelle base, archiver l'ancienne.
4. Les autres postes repartent d'un `since=0` (réplication complète) depuis le premier qui a poussé.

> À faire **avant** le premier échange multi-postes, sinon collisions d'IDs entiers.

---

## 10. Décisions ouvertes (à trancher, pas maintenant)

- **Un journal global vs. un journal par domaine** → commencer par *un par domaine* (isolation, curseurs indépendants). Simple à fusionner plus tard.
- **Purge des tombstones** : après quel délai ? (proposition : jamais automatique en v1, commande de maintenance manuelle).
- **Compaction du journal** : quand `seq` devient énorme (non urgent).
- **Transport** : HTTP/JSON en v1. gRPC/binaire seulement si le volume l'exige (il ne l'exigera pas avant longtemps).

---

## Résumé en une phrase

> Chaque entité porte `{id(uuid), rev, updatedAt, deleted, origin}` ; le client
> travaille en local et, quand le serveur est là, il **PUSH ses changements puis
> PULL depuis son curseur `lastSeq`** ; le serveur attribue un `seq` monotone qui
> **ordonne tout** et **arbitre les conflits en last-write-wins** — sans jamais
> comparer les horloges des machines.
