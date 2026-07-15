/**
 * change_store.h — Le journal ordonné d'un domaine (cœur du protocole).
 *
 * Un ChangeStore = un domaine (componenthub, meteohub…). Il détient l'unique
 * numéro de séquence monotone qui ORDONNE et ARBITRE tout (docs/sync-contract.md
 * §2 et §3). On ne conserve que le dernier état par entité (LWW) : suffisant pour
 * la réplication, aucun historique complet nécessaire en v1.
 *
 * Persistance : un fichier JSON par domaine ({dataDir}/{domain}.json).
 * Thread-safe : le serveur HTTP traite chaque requête sur son propre thread.
 */

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include "change.h"

namespace hsh {

// Résultat d'un PULL : les changements + le curseur + la pagination.
struct PullResult {
    std::vector<Change> changes;
    std::int64_t lastSeq = 0;
    bool hasMore = false;
};

// Un id écrasé alors que le hub connaissait une révision plus récente (§3).
struct Conflict {
    std::string id;
    std::int64_t serverRev = 0;
    std::int64_t yourRev = 0;
};

// Résultat d'un PUSH.
struct PushResult {
    std::vector<std::pair<std::string, std::int64_t>> applied; // (id, seq attribué)
    std::vector<Conflict> conflicts;
    std::int64_t lastSeq = 0;
};

class ChangeStore {
public:
    // Charge le journal depuis {dataDir}/{domain}.json (créé vide si absent).
    ChangeStore(std::string domain, std::string filePath);

    const std::string& domain() const { return domain_; }
    const std::string& journalId() const { return journalId_; }  // « époque » du journal
    std::int64_t lastSeq() const;
    std::size_t count() const;   // nombre d'entités connues (tombstones inclus)

    // PULL : tous les changements dont seq > since, triés par seq croissant,
    // plafonnés à `limit` (0 = pas de limite).
    PullResult changesSince(std::int64_t since, std::size_t limit) const;

    // PUSH : intègre un lot de changements clients. Attribue un seq à chacun,
    // applique le last-write-wins par arrivée, signale les écrasements (§3).
    PushResult apply(const std::vector<Change>& incoming);

private:
    void load();
    void save() const;   // appelé sous verrou

    std::string domain_;
    std::string journalId_;   // identité stable du journal (« époque »)
    std::string filePath_;
    mutable std::mutex mutex_;
    std::int64_t nextSeq_ = 0;
    std::unordered_map<std::string, Change> byId_; // dernier état par entité
};

} // namespace hsh
