/**
 * beacon.h — émetteur morfbeacon/1, sans dépendance.
 *
 * morfSync est le premier service du parc : il précède le protocole que les
 * autres respectent, et il est resté invisible dans l'onglet Écosystème de
 * morfMonitor — non pas en panne, jamais découvert.
 *
 * Pourquoi ne PAS vendoriser morfBeacon ici, alors que tous les autres le font.
 * morfBeacon exige Qt6 Core et Network. morfSync n'a aucune dépendance Qt : il
 * tourne sur nlohmann_json et des sockets brutes, et c'est ce qui en fait le
 * service le plus léger du parc. Importer un framework entier pour émettre un
 * datagramme et servir une route JSON coûterait plus que ça ne rapporte.
 *
 * Le parc a déjà tranché ce cas : arduino/morfbeacon_emitter.h, dans morfBeacon,
 * est une implémentation sans dépendance écrite pour l'ESP32, qui ne peut pas
 * embarquer Qt non plus. **Le contrat, c'est le protocole, pas la bibliothèque.**
 * Ce fichier est la troisième implémentation du même contrat, et elle se valide
 * comme les autres : morfBeacon/tools/check-protocol.py écoute le LAN et vérifie
 * ce qui passe réellement sur le réseau.
 */

#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace hsh {

/// Ce qu'un service annonce de lui-même. Rempli une fois au démarrage.
struct BeaconIdentity {
    std::string app;            ///< nom lisible, ex. "morfSync"
    std::string version;        ///< ex. "0.2.9"
    std::string instance;       ///< identifiant stable de CETTE installation
    unsigned short statusPort;  ///< port où /status répond
};

/// Réglages d'émission. Les défauts sont ceux du parc.
struct BeaconSettings {
    bool enabled = true;
    unsigned short udpPort = 45454;
    int intervalMs = 15000;
};

/**
 * Émetteur de présence : diffuse un datagramme JSON court à intervalle fixe.
 *
 * « Push presence / pull detail » : le heartbeat dit seulement qu'un service
 * existe et où l'interroger. Tout le reste vit derrière /status, que le
 * consommateur tire quand il en a besoin. C'est pourquoi le datagramme doit
 * rester sous 512 octets.
 */
class Beacon {
public:
    Beacon(BeaconIdentity identity, BeaconSettings settings);
    ~Beacon();

    Beacon(const Beacon&) = delete;
    Beacon& operator=(const Beacon&) = delete;

    /// Démarre l'émission en tâche de fond. Sans effet si désactivé.
    void start();

    /// Arrête proprement ; appelé aussi par le destructeur.
    void stop();

    /// Le corps du datagramme, exposé pour que /status et le heartbeat ne
    /// puissent pas diverger sur l'identité qu'ils annoncent.
    std::string identityJson(long long uptimeSeconds) const;

    /// Secondes écoulées depuis la construction.
    long long uptimeSeconds() const;

private:
    void loop();

    BeaconIdentity identity_;
    BeaconSettings settings_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    long long startedAt_ = 0;
};

/**
 * Un identifiant stable pour cette installation.
 *
 * Stable au sens du protocole : deux redémarrages doivent produire la même
 * valeur, sinon un consommateur croit voir deux services apparaître et
 * disparaître. Dérivé du nom d'hôte et du port, donc reproductible sans rien
 * écrire sur le disque.
 */
std::string makeInstanceId(const std::string& app, unsigned short port);

/// Le nom d'hôte de la machine, ou "unknown" si le système ne le donne pas.
std::string hostName();

}  // namespace hsh
