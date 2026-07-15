#include "change_store.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <random>
#include <cstdio>
#include <cstdint>

namespace hsh {

namespace {
// Identité d'un journal (« époque ») : générée une fois à la création d'un
// journal, persistée. Si le fichier disparaît (dossier déplacé/effacé), un
// nouveau journalId est émis -> les clients détectent le changement et
// réinitialisent leur curseur au lieu de rater silencieusement les changements.
std::string generateJournalId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;
    char buf[33];
    std::snprintf(buf, sizeof(buf), "%016llx%016llx",
                  static_cast<unsigned long long>(dist(rng)),
                  static_cast<unsigned long long>(dist(rng)));
    return buf;
}
} // namespace

ChangeStore::ChangeStore(std::string domain, std::string filePath)
    : domain_(std::move(domain)), filePath_(std::move(filePath)) {
    load();
    if (journalId_.empty()) journalId_ = generateJournalId();
}

void ChangeStore::load() {
    std::ifstream in(filePath_);
    if (!in.is_open()) return; // journal neuf : rien à charger

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception&) {
        return; // fichier corrompu : on repart d'un journal vide plutôt que de crasher
    }

    nextSeq_ = j.value("nextSeq", static_cast<std::int64_t>(0));
    journalId_ = j.value("journalId", std::string());
    if (j.contains("changes") && j["changes"].is_array()) {
        for (const auto& item : j["changes"]) {
            Change c = Change::fromJson(item);
            if (!c.id.empty()) byId_[c.id] = c;
        }
    }
}

void ChangeStore::save() const {
    nlohmann::json changes = nlohmann::json::array();
    for (const auto& [id, c] : byId_) changes.push_back(c.toJson());

    nlohmann::json j{{"domain", domain_}, {"journalId", journalId_},
                     {"nextSeq", nextSeq_}, {"changes", changes}};

    // Écriture atomique : fichier temporaire puis renommage, pour ne jamais
    // laisser un journal tronqué si le process meurt en cours d'écriture.
    const std::string tmp = filePath_ + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << j.dump(2);
    }
    std::error_code ec;
    std::filesystem::rename(tmp, filePath_, ec);
    if (ec) { // rename a échoué (ex. cible ouverte) : repli en copie directe
        std::ofstream out(filePath_, std::ios::trunc);
        out << j.dump(2);
        std::filesystem::remove(tmp, ec);
    }
}

std::int64_t ChangeStore::lastSeq() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::int64_t last = 0;
    for (const auto& [id, c] : byId_) last = std::max(last, c.seq);
    return last;
}

std::size_t ChangeStore::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return byId_.size();
}

PullResult ChangeStore::changesSince(std::int64_t since, std::size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Change> selected;
    for (const auto& [id, c] : byId_)
        if (c.seq > since) selected.push_back(c);

    std::sort(selected.begin(), selected.end(),
              [](const Change& a, const Change& b) { return a.seq < b.seq; });

    PullResult result;
    if (limit > 0 && selected.size() > limit) {
        result.hasMore = true;
        selected.resize(limit);
    }
    result.changes = std::move(selected);
    result.lastSeq = result.changes.empty() ? since : result.changes.back().seq;
    return result;
}

PushResult ChangeStore::apply(const std::vector<Change>& incoming) {
    std::lock_guard<std::mutex> lock(mutex_);

    PushResult result;
    bool changed = false;
    for (Change c : incoming) {
        if (c.id.empty()) continue; // un changement sans identité est ignoré

        auto it = byId_.find(c.id);
        if (it != byId_.end()) {
            const Change& ex = it->second;

            // Renvoi à l'identique (même révision et même horodatage) : no-op
            // idempotent. Permet à un client de re-pousser tout son jeu à chaque
            // synchro sans générer de nouveaux seq ni de bruit au PULL.
            if (c.rev == ex.rev && c.updatedAt == ex.updatedAt) {
                result.applied.push_back({c.id, ex.seq});
                continue;
            }

            // Résolution déterministe : gagne la révision la plus haute, puis à
            // révision égale l'horodatage le plus récent (ISO 8601 -> ordre
            // lexicographique = chronologique). Une version plus ancienne ne peut
            // pas écraser une plus récente : elle est refusée et signalée (§3).
            const bool incomingNewer =
                (c.rev > ex.rev) || (c.rev == ex.rev && c.updatedAt > ex.updatedAt);
            if (!incomingNewer) {
                result.conflicts.push_back({c.id, ex.rev, c.rev});
                continue;
            }
        }

        c.seq = ++nextSeq_;      // le hub attribue l'ordre
        byId_[c.id] = c;         // la version (strictement) plus récente remplace
        result.applied.push_back({c.id, c.seq});
        changed = true;
    }

    std::int64_t last = 0;
    for (const auto& [id, c] : byId_) last = std::max(last, c.seq);
    result.lastSeq = last;

    if (changed) save();
    return result;
}

} // namespace hsh
