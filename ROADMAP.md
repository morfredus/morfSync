# Feuille de route — morfSync

> Le hub grandit par petits paliers. On ne complexifie pas tant que le socle ne
> fait pas mal.

## v0.1 — Socle (fait)
- [x] Serveur HTTP minimal cross-plateforme (win-x64 / Linux / ARM64).
- [x] Journal ordonné par domaine : seq monotone, LWW, tombstones, pagination.
- [x] Endpoints du contrat : health, PULL, PUSH.
- [x] Auth optionnelle par jeton partagé.
- [x] Test de fumée du journal.

## v0.2 — Premier client réel
- [ ] Piloter avec **ComponentHub** : migration `int → UUID`, enveloppe `SyncMeta`,
      implémentation `SyncStorage` par-dessus `JsonStorage`.
- [ ] Boucle de synchro client (PUSH puis PULL depuis curseur).
- [ ] Journalisation des conflits côté client (visibilité, sans blocage).

## v0.3 — Confort réseau
- [ ] Découverte mDNS (`morfsync.local`) avec repli IP manuelle.
- [ ] Page d'état minimale (`GET /` : domaines, dernier seq, nb d'entités).
- [ ] Profil client léger **ESP32** (MeteoHub / morfBeacon) : PUSH mesures, PULL config.

## Plus tard (à trancher, pas maintenant)
- [ ] Backend `SqliteStorage` derrière la même interface.
- [ ] Purge/compaction des tombstones (opération de maintenance).
- [ ] Services communs : sauvegardes, notifications, mises à jour.
- [ ] Résolution de conflits avancée (choix utilisateur, conservation des deux versions).
