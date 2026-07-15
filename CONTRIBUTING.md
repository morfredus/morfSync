# Contribuer à HomeServerHub

## Principe directeur
HomeServerHub reste **léger et agnostique au métier**. Il transporte des
enveloppes de synchronisation ; il n'interprète jamais leur contenu. Toute
évolution doit respecter [docs/sync-contract.md](docs/sync-contract.md) — le
contrat est la source de vérité partagée par tous les clients de l'écosystème.

## Avant de coder
- Une modification du **format d'échange** (enveloppe, endpoints) = une révision
  du contrat d'abord, discutée, puis le code.
- Pas de nouvelle dépendance externe sans raison forte. La cible est : compilable
  sur win-x64 / Linux / ARM64 avec seulement CMake, un compilateur C++17 et
  `nlohmann_json`.

## Build & tests
```bash
cmake --preset linux -DHSH_BUILD_SMOKE=ON
cmake --build --preset linux
ctest --preset linux
```
Le test de fumée (`test/smoke_main.cpp`) valide les invariants du journal sans
réseau : toute évolution de `ChangeStore` doit le garder vert et, idéalement,
ajouter les cas correspondants.

## Style
- C++17, identifiants en anglais, commentaires en français (comme le reste de
  l'écosystème). On explique le *pourquoi*, pas le *quoi*.
- Versionnage SemVer, entrée dans `CHANGELOG.md` à chaque changement notable.

## Licence
En contribuant, vous acceptez que votre contribution soit distribuée sous
GPL-3.0-only.
