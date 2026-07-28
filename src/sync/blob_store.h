/**
 * blob_store.h — Magasin de blobs adressé par contenu.
 *
 * Un blob = un fichier nommé par le hash (SHA-256, 64 hex) de son contenu, rangé
 * sous {dataDir}/blobs/. Sert à transporter le BINAIRE des pièces jointes entre
 * postes sans alourdir le journal de changements : celui-ci ne garde que la
 * référence de hash, le contenu vit ici et se réclame à la demande.
 *
 * Le hub traite le hash comme une CLÉ OPAQUE : il ne recalcule PAS le SHA-256
 * (morfSync reste sans dépendance crypto). L'intégrité est garantie de bout en
 * bout par le CLIENT, qui vérifie le hash de ce qu'il télécharge. Sur un LAN de
 * confiance à token partagé, c'est le bon compromis : le hub est un seau adressé
 * par contenu, générique, réutilisable par n'importe quel client.
 *
 * En-tête seul (inline) : pas de source à ajouter au CMake, à l'image des types
 * de valeur du domaine.
 */

#pragma once
#include <string>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <atomic>
#include <cctype>
#include <iterator>

namespace hsh {

class BlobStore {
public:
    explicit BlobStore(const std::string& dataDir)
        : dir_(std::filesystem::path(dataDir) / "blobs") {
        std::error_code ec;
        std::filesystem::create_directories(dir_, ec);
    }

    // 64 caractères hexadécimaux. Le format seul suffit à écarter tout ce qui
    // pourrait s'échapper du dossier (« / », « .. », séparateurs) : un nom qui
    // n'est QUE de l'hexadécimal ne contient aucun de ces caractères.
    static bool validHash(const std::string& h) {
        if (h.size() != 64) return false;
        for (char c : h)
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        return true;
    }

    bool has(const std::string& hash) const {
        std::error_code ec;
        return std::filesystem::exists(path(hash), ec);
    }

    bool get(const std::string& hash, std::string& out) const {
        std::ifstream in(path(hash), std::ios::binary);
        if (!in) return false;
        out.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
        return true;
    }

    // Idempotent : si le blob existe déjà (donc le même contenu, par
    // construction adressée par hash), c'est un no-op. Écriture atomique
    // (fichier temporaire puis rename) pour qu'un GET concurrent ne voie jamais
    // un blob à moitié écrit.
    bool put(const std::string& hash, const std::string& bytes) {
        if (has(hash)) return true;
        const std::filesystem::path target = path(hash);
        const std::filesystem::path tmp =
            std::filesystem::path(target.string() + ".tmp." + std::to_string(nextTmp()));
        {
            std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
            if (!o) return false;
            o.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            o.flush();
            if (!o) { std::error_code rm; std::filesystem::remove(tmp, rm); return false; }
        }
        std::error_code ec;
        std::filesystem::rename(tmp, target, ec);
        if (ec) {
            // Course : un autre thread a peut-être posé le même blob entre-temps.
            // Le contenu étant identique (même hash), c'est un succès si la cible
            // est désormais présente.
            std::error_code rm; std::filesystem::remove(tmp, rm);
            return has(hash);
        }
        return true;
    }

private:
    std::filesystem::path path(const std::string& hash) const { return dir_ / hash; }

    static long long nextTmp() {
        static std::atomic<long long> n{0};
        return ++n;
    }

    std::filesystem::path dir_;
};

} // namespace hsh
