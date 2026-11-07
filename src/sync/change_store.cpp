#include "change_store.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <optional>
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

    try {
        nlohmann::json j;
        in >> j;
        const std::int64_t nextSeq = j.value("nextSeq", static_cast<std::int64_t>(0));
        const std::string journalId = j.value("journalId", std::string());
        std::unordered_map<std::string, Change> loaded;
        if (j.contains("changes") && j["changes"].is_array()) {
            for (const auto& item : j["changes"]) {
                Change c = Change::fromJson(item);
                if (!c.id.empty()) loaded[c.id] = c;
            }
        }
        nextSeq_ = nextSeq;
        journalId_ = journalId;
        byId_ = std::move(loaded);
    } catch (const std::exception&) {
        // Fichier illisible : ne PAS repartir d'un journal vide en l'ecrasant au prochain
        // save (domaine perdu). Le mettre de cote en .corrupt pour une reprise manuelle ;
        // un nouveau journalId est emis par le constructeur, les clients se resynchronisent.
        in.close();
        std::error_code ec;
        const std::string corrupt = filePath_ + ".corrupt";
        std::filesystem::remove(corrupt, ec);
        std::filesystem::rename(filePath_, corrupt, ec);
        std::fprintf(stderr, "[morfSync] journal illisible, mis de cote : %s\n", corrupt.c_str());
        std::fflush(stderr);
        nextSeq_ = 0;
        journalId_.clear();
        byId_.clear();
    }
}

bool ChangeStore::save() const {
    nlohmann::json changes = nlohmann::json::array();
    for (const auto& [id, c] : byId_) changes.push_back(c.toJson());

    nlohmann::json j{{"domain", domain_}, {"journalId", journalId_},
                     {"nextSeq", nextSeq_}, {"changes", changes}};

    // Écriture atomique : fichier temporaire puis renommage, pour ne jamais
    // laisser un journal tronqué si le process meurt en cours d'écriture.
    // Le flux est verifie APRES flush/fermeture : disque plein ou quota se manifestent
    // a ce moment. En cas d'echec, le .tmp est supprime et l'ancien fichier reste en place
    // (aucun repli en ecriture directe, qui tronquerait le journal existant).
    const std::string tmp = filePath_ + ".tmp";
    std::error_code ec;
    {
        std::ofstream out(tmp, std::ios::trunc | std::ios::binary);
        out << j.dump(2);
        out.flush();
        if (!out.good()) {
            out.close();
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }
    std::filesystem::rename(tmp, filePath_, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
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
    // Journal d'annulation : si l'ecriture disque echoue, le lot est defait pour que la
    // memoire ne diverge pas du fichier (le client re-poussera et obtiendra de vrais seq).
    const std::int64_t seqBefore = nextSeq_;
    std::vector<std::pair<std::string, std::optional<Change>>> undo;
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
        {
            auto prev = byId_.find(c.id);
            undo.emplace_back(c.id, prev == byId_.end() ? std::nullopt
                                                         : std::optional<Change>(prev->second));
        }
        byId_[c.id] = c;         // la version (strictement) plus récente remplace
        result.applied.push_back({c.id, c.seq});
        changed = true;
    }

    std::int64_t last = 0;
    for (const auto& [id, c] : byId_) last = std::max(last, c.seq);
    result.lastSeq = last;

    if (changed && !save()) {
        for (auto it = undo.rbegin(); it != undo.rend(); ++it) {
            if (it->second) byId_[it->first] = *it->second;
            else byId_.erase(it->first);
        }
        nextSeq_ = seqBefore;
        result.applied.clear();
        result.persisted = false;
        std::int64_t restored = 0;
        for (const auto& [id, c] : byId_) restored = std::max(restored, c.seq);
        result.lastSeq = restored;
    }
    return result;
}

bool ChangeStore::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto oldById = std::move(byId_);
    const std::int64_t oldSeq = nextSeq_;
    const std::string oldJournalId = journalId_;
    byId_.clear();
    nextSeq_ = 0;
    journalId_ = generateJournalId();
    if (save())
        return true;
    byId_ = std::move(oldById);
    nextSeq_ = oldSeq;
    journalId_ = oldJournalId;
    return false;
}

} // namespace hsh
