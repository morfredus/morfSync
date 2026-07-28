# Emplacements des fichiers - morfSync

morfSync suit la doctrine du parc morfSystem (référence complète :
`morfTemplateService/docs/fr/FILESYSTEM.md`). Ce document précise sa disposition.

## Les trois zones

| Zone | Contenu | Emplacement (Linux) | Écriture |
|------|---------|---------------------|----------|
| Programme | binaire `morfSync` | `/opt/morfsync` | non |
| Config admin | `morfsync.json` | `/etc/morfsystem/morfsync` | non |
| État persistant | journaux de synchro par domaine | `/var/lib/morfsystem/morfsync` | oui |

Sous Windows, l'état se replie sous `%ProgramData%\morfsystem\morfsync\state`.

## L'état persistant

Les journaux de synchro (`{domain}.json`, un par domaine) sont la source de
vérité du service. C'est de l'**état généré** : personne ne les ouvre à la main,
toutes les opérations passent par les consommateurs via le réseau. Ils vivent
donc sous `/var/lib`, jamais dans `/opt` (le programme) ni dans `/etc` (la config
admin).

L'unité systemd déclare `StateDirectory=morfsystem/morfsync` : systemd crée
`/var/lib/morfsystem/morfsync`, possédé par l'utilisateur du service et
accessible en écriture, puis l'expose via `$STATE_DIRECTORY`. Le service utilise
cette variable comme dossier de données par défaut (voir `src/app/paths.cpp`).
Plus besoin de provisionner les droits à la main.

## Surcharge

La clé `dataDir` du fichier de configuration reste prioritaire sur le défaut :
elle sert à placer les données sur un autre volume, ou à pointer un ancien
emplacement lors d'une migration.

## Migration depuis une version antérieure

Une installation antérieure à 0.5.0 range ses données sous `/opt/morfsync/data`.
Les déplacer avant de démarrer la nouvelle version :

```bash
sudo systemctl stop morfsync
sudo mv /opt/morfsync/data/* /var/lib/morfsystem/morfsync/ 2>/dev/null || true
sudo systemctl start morfsync
```

Autre option : fixer `"dataDir": "/opt/morfsync/data"` dans la configuration pour
conserver l'ancien emplacement.
