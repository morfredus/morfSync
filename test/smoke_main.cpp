/**
 * smoke_main.cpp — Test de fumée headless du journal (aucun réseau).
 *
 * Vérifie les invariants du contrat : attribution de seq monotone, last-write-wins
 * par id, détection d'écrasement (conflit), pagination du PULL et persistance.
 * Compilé via l'option CMake MS_BUILD_SMOKE. Retourne 0 si tout passe.
 */

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>

#include "sync/change_store.h"

using hsh::Change;
using hsh::ChangeStore;

static Change mk(const std::string& id, std::int64_t rev, bool deleted = false) {
    Change c;
    c.id = id;
    c.type = "component";
    c.rev = rev;
    c.deleted = deleted;
    c.origin = "test-device";
    c.data = {{"name", "sample"}};
    return c;
}

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } \
                              else { std::printf("ok  : %s\n", msg); } } while (0)

int main() {
    const std::string file =
        (std::filesystem::temp_directory_path() / "hsh_smoke_store.json").string();
    std::filesystem::remove(file);

    {
        ChangeStore store("componenthub", file);

        // 1) PUSH initial : deux entités -> seq 1 et 2.
        auto r1 = store.apply({mk("A", 1), mk("B", 1)});
        CHECK(r1.applied.size() == 2, "push de 2 changements -> 2 appliqués");
        CHECK(r1.applied[0].second == 1 && r1.applied[1].second == 2, "seq monotone 1,2");
        CHECK(r1.conflicts.empty(), "aucun conflit au premier push");

        // 2) PULL depuis 0 : on récupère les 2.
        auto p0 = store.changesSince(0, 0);
        CHECK(p0.changes.size() == 2, "pull since=0 -> 2 changements");
        CHECK(p0.lastSeq == 2, "pull lastSeq=2");

        // 2bis) Re-push à l'identique (même rev + updatedAt) : no-op idempotent,
        // aucun nouveau seq (le client peut re-pousser tout son jeu sans bruit).
        auto rIdem = store.apply({mk("A", 1), mk("B", 1)});
        CHECK(rIdem.applied.size() == 2, "re-push identique -> appliqué (echo)");
        CHECK(rIdem.lastSeq == 2, "re-push identique -> lastSeq inchangé (pas de nouveau seq)");
        CHECK(store.changesSince(2, 0).changes.empty(), "re-push identique -> rien de neuf au pull");

        // 3) LWW : re-push de A avec rev supérieure -> nouveau seq, remplace.
        auto r2 = store.apply({mk("A", 2)});
        CHECK(r2.applied[0].second == 3, "re-push A -> seq 3 (monotone)");
        CHECK(r2.conflicts.empty(), "rev croissante -> pas de conflit");

        // 4) PULL incrémental depuis le curseur : seul A (seq 3) revient.
        auto p2 = store.changesSince(2, 0);
        CHECK(p2.changes.size() == 1 && p2.changes[0].id == "A", "pull since=2 -> seulement A");
        CHECK(p2.changes[0].rev == 2, "A a bien rev=2 après LWW");

        // 5) Conflit : push de A avec rev INFÉRIEURE à la connue -> signalé.
        auto r3 = store.apply({mk("A", 1)});
        CHECK(r3.conflicts.size() == 1, "push rev<connue -> 1 conflit signalé");
        CHECK(r3.conflicts[0].serverRev == 2 && r3.conflicts[0].yourRev == 1, "conflit rapporte serverRev/yourRev");

        // 6) Tombstone : suppression logique transportée comme un changement.
        auto r4 = store.apply({mk("B", 2, /*deleted=*/true)});
        CHECK(r4.applied.size() == 1, "tombstone appliqué comme un changement");

        // 7) Pagination.
        auto pg = store.changesSince(0, 1);
        CHECK(pg.changes.size() == 1 && pg.hasMore, "limit=1 -> hasMore=true");
    }

    // 8) Persistance : recharger depuis le fichier conserve l'état et le seq.
    {
        ChangeStore reloaded("componenthub", file);
        auto all = reloaded.changesSince(0, 0);
        CHECK(all.changes.size() == 2, "rechargement conserve 2 entités (A,B)");
        auto after = reloaded.apply({mk("C", 1)});
        CHECK(after.applied[0].second > all.lastSeq, "seq continue après rechargement (pas de réutilisation)");
    }

    std::filesystem::remove(file);
    std::printf(failures ? "\n%d test(s) en échec\n" : "\nTous les tests passent\n", failures);
    return failures == 0 ? 0 : 1;
}
