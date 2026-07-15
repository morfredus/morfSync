/**
 * change.h — L'enveloppe de synchronisation.
 *
 * morfSync est volontairement AGNOSTIQUE au métier : il ne connaît ni les
 * composants de ComponentHub, ni les mesures de MeteoHub. Il ne manipule que
 * cette enveloppe (voir docs/sync-contract.md, §1). La charge utile métier
 * ("data") reste un JSON opaque que le hub transporte sans l'interpréter.
 */

#pragma once
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace hsh {

// Un changement tel que stocké dans le journal d'un domaine.
// Le hub y ajoute "seq" ; tout le reste vient du client.
struct Change {
    std::int64_t seq = 0;        // numéro de séquence monotone attribué par le hub
    std::string  id;             // UUID permanent de l'entité (clé de LWW)
    std::string  type;           // discriminant d'entité ("component", "location"…)
    bool         deleted = false;// tombstone
    std::int64_t rev = 0;        // révision locale, incrémentée par le client à chaque save
    std::string  origin;         // deviceId ayant produit cette révision
    std::string  createdAt;      // ISO 8601 UTC — affichage seulement
    std::string  updatedAt;      // ISO 8601 UTC — affichage seulement
    nlohmann::json data;         // charge utile métier, opaque pour le hub

    // Sérialisation vers le format d'échange / de stockage (identiques).
    nlohmann::json toJson() const {
        return nlohmann::json{
            {"seq", seq}, {"id", id}, {"type", type}, {"deleted", deleted},
            {"rev", rev}, {"origin", origin},
            {"createdAt", createdAt}, {"updatedAt", updatedAt},
            {"data", data}};
    }

    static Change fromJson(const nlohmann::json& j) {
        Change c;
        c.seq       = j.value("seq", static_cast<std::int64_t>(0));
        c.id        = j.value("id", std::string{});
        c.type      = j.value("type", std::string{});
        c.deleted   = j.value("deleted", false);
        c.rev       = j.value("rev", static_cast<std::int64_t>(0));
        c.origin    = j.value("origin", std::string{});
        c.createdAt = j.value("createdAt", std::string{});
        c.updatedAt = j.value("updatedAt", std::string{});
        c.data      = j.contains("data") ? j.at("data") : nlohmann::json::object();
        return c;
    }
};

} // namespace hsh
