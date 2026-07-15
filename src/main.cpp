/**
 * main.cpp — HomeServerHub : point d'entrée.
 *
 * Câble les endpoints du contrat de synchronisation (docs/sync-contract.md §4)
 * au serveur HTTP minimal et au journal de changements. Le hub reste agnostique
 * au métier : chaque {domain} de l'URL obtient son propre ChangeStore, créé à la
 * demande et persisté dans {dataDir}/{domain}.json.
 */

#include <iostream>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <filesystem>
#include <ctime>

#include <nlohmann/json.hpp>

#include "net/http_server.h"
#include "sync/change_store.h"
#include "app/config.h"
#include "app/paths.h"

#ifndef HSH_VERSION
#define HSH_VERSION "0.0.0"
#endif

using nlohmann::json;

namespace {

std::string nowIso8601Utc() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

// Un nom de domaine sûr pour un chemin de fichier (évite ../ et séparateurs).
bool validDomain(const std::string& d) {
    if (d.empty() || d.size() > 64) return false;
    for (char c : d)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'))
            return false;
    return true;
}

// Registre des domaines : un ChangeStore par domaine (fichier {domain}.json).
class Registry {
public:
    explicit Registry(std::string dataDir) : dataDir_(std::move(dataDir)) {
        std::error_code ec;
        std::filesystem::create_directories(dataDir_, ec);  // writability déjà vérifiée par main
    }

    hsh::ChangeStore& store(const std::string& domain) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stores_.find(domain);
        if (it == stores_.end()) {
            const std::string path = dataDir_ + "/" + domain + ".json";
            it = stores_.emplace(domain,
                     std::make_unique<hsh::ChangeStore>(domain, path)).first;
        }
        return *it->second;
    }

    // Charge au démarrage tous les journaux déjà présents sur le disque, pour
    // que /api/status les reflète et que les données survivent aux redémarrages.
    // Appelé avant le démarrage du serveur (mono-thread) : pas de verrou externe.
    int preload() {
        int loaded = 0;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dataDir_, ec)) {
            if (ec) break;
            const auto& p = entry.path();
            if (!entry.is_regular_file() || p.extension() != ".json") continue;
            const std::string domain = p.stem().string();
            if (validDomain(domain)) { store(domain); ++loaded; }
        }
        return loaded;
    }

    // Instantané de l'état : un objet par domaine {domain, count, lastSeq}.
    nlohmann::json statusJson() {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json arr = nlohmann::json::array();
        for (auto& [name, s] : stores_)
            arr.push_back({{"domain", name}, {"count", s->count()},
                           {"lastSeq", s->lastSeq()}, {"journalId", s->journalId()}});
        return arr;
    }

    const std::string& dataDir() const { return dataDir_; }

private:
    std::string dataDir_;
    std::mutex mutex_;
    std::map<std::string, std::unique_ptr<hsh::ChangeStore>> stores_;
};

} // namespace

int main(int argc, char** argv) {
    const std::string configPath = (argc > 1) ? argv[1] : "config.json";

    hsh::Config cfg;
    std::string cfgError;
    if (!hsh::Config::loadFromFile(configPath, cfg, cfgError)) {
        std::cerr << "[HomeServerHub] " << cfgError << "\n";
        return 1;
    }

    // dataDir non fixé -> emplacement par défaut conforme à l'OS (paths.h).
    if (cfg.dataDir.empty()) cfg.dataDir = hsh::defaultDataDir();

    // Vérifie que le dossier de données est CRÉABLE et ACCESSIBLE EN ÉCRITURE
    // avant tout : sinon on affiche une erreur claire et on sort proprement,
    // au lieu de planter en boucle (ex. dataDir hérité pointant vers /var/lib
    // alors que le service tourne sous un compte non privilégié).
    {
        std::error_code ec;
        std::filesystem::create_directories(cfg.dataDir, ec);
        const std::filesystem::path probe =
            std::filesystem::path(cfg.dataDir) / ".hsh_write_test";
        bool writable = false;
        { std::ofstream t(probe); writable = t.good(); }
        std::error_code rm; std::filesystem::remove(probe, rm);
        if (!writable) {
            std::cerr << "[HomeServerHub] dossier de données inaccessible en écriture : "
                      << cfg.dataDir;
            if (ec) std::cerr << " (" << ec.message() << ")";
            std::cerr << "\n  Corrigez \"dataDir\" dans " << configPath
                      << " (ou supprimez cette clé pour l'emplacement par défaut), puis redémarrez."
                      << std::endl;
            return 1;
        }
    }

    Registry registry(cfg.dataDir);
    const int preloaded = registry.preload();
    hsh::HttpServer server(cfg.host, cfg.port);

    // Vérifie le Bearer partagé si un token est configuré. Renvoie true si OK.
    auto authorized = [&cfg](const hsh::HttpRequest& req) {
        if (cfg.token.empty()) return true; // auth désactivée (LAN de confiance)
        const std::string h = req.header("authorization");
        return h == "Bearer " + cfg.token;
    };

    // --- GET /api/health : état du serveur (ouvert, sans auth) --------------
    server.route("GET", "/api/health", [](const hsh::HttpRequest&) {
        json body{{"status", "ok"}, {"time", nowIso8601Utc()}, {"version", HSH_VERSION}};
        return hsh::HttpResponse::json(200, body.dump());
    });

    // --- GET /api/status : domaines connus + nb d'entités + curseur ---------
    server.route("GET", "/api/status", [&registry](const hsh::HttpRequest&) {
        json body{{"status", "ok"}, {"time", nowIso8601Utc()}, {"version", HSH_VERSION},
                  {"domains", registry.statusJson()}};
        return hsh::HttpResponse::json(200, body.dump());
    });

    // --- GET /api/:domain/changes?since=&limit= : PULL ---------------------
    server.route("GET", "/api/:domain/changes",
                 [&registry, &authorized](const hsh::HttpRequest& req) {
        if (!authorized(req))
            return hsh::HttpResponse::json(401, R"({"error":"unauthorized"})");
        const std::string domain = req.param("domain");
        if (!validDomain(domain))
            return hsh::HttpResponse::json(400, R"({"error":"invalid domain"})");

        std::int64_t since = std::strtoll(req.queryParam("since", "0").c_str(), nullptr, 10);
        std::size_t  limit = static_cast<std::size_t>(
                                 std::strtoul(req.queryParam("limit", "0").c_str(), nullptr, 10));

        hsh::ChangeStore& store = registry.store(domain);
        hsh::PullResult pull = store.changesSince(since, limit);
        json changes = json::array();
        for (const auto& c : pull.changes) changes.push_back(c.toJson());
        json body{{"changes", changes}, {"lastSeq", pull.lastSeq}, {"hasMore", pull.hasMore},
                  {"journalId", store.journalId()}};  // « époque » : reset côté client si elle change
        return hsh::HttpResponse::json(200, body.dump());
    });

    // --- POST /api/:domain/changes : PUSH ----------------------------------
    server.route("POST", "/api/:domain/changes",
                 [&registry, &authorized](const hsh::HttpRequest& req) {
        if (!authorized(req))
            return hsh::HttpResponse::json(401, R"({"error":"unauthorized"})");
        const std::string domain = req.param("domain");
        if (!validDomain(domain))
            return hsh::HttpResponse::json(400, R"({"error":"invalid domain"})");

        json in;
        try {
            in = json::parse(req.body);
        } catch (const std::exception& e) {
            return hsh::HttpResponse::json(400,
                json{{"error", std::string("invalid json: ") + e.what()}}.dump());
        }

        std::vector<hsh::Change> incoming;
        if (in.contains("changes") && in["changes"].is_array())
            for (const auto& item : in["changes"])
                incoming.push_back(hsh::Change::fromJson(item));

        hsh::PushResult res = registry.store(domain).apply(incoming);

        json applied = json::array();
        for (const auto& [id, seq] : res.applied)
            applied.push_back(json{{"id", id}, {"seq", seq}});
        json conflicts = json::array();
        for (const auto& c : res.conflicts)
            conflicts.push_back(json{{"id", c.id}, {"serverRev", c.serverRev}, {"yourRev", c.yourRev}});

        json body{{"applied", applied}, {"conflicts", conflicts}, {"lastSeq", res.lastSeq}};
        return hsh::HttpResponse::json(200, body.dump());
    });

    std::cout << "HomeServerHub " << HSH_VERSION
              << " — écoute sur http://" << cfg.host << ":" << cfg.port << "\n"
              << "  données   : " << std::filesystem::absolute(cfg.dataDir).string() << "\n"
              << "  domaines  : " << preloaded << " chargé(s) au démarrage\n"
              << "  auth      : " << (cfg.token.empty() ? "désactivée" : "Bearer") << std::endl;

    if (!server.run()) {
        std::cerr << "[HomeServerHub] impossible d'ouvrir le port " << cfg.port
                  << " (déjà utilisé ?)\n";
        return 1;
    }
    return 0;
}
