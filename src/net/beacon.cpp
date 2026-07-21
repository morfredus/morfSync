/**
 * beacon.cpp — implémentation de l'émetteur morfbeacon/1.
 *
 * Voir beacon.h pour la raison de ne pas vendoriser morfBeacon ici.
 */

#include "net/beacon.h"

#include <chrono>
#include <cstring>
#include <ctime>
#include <sstream>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  static int closeSocket(socket_t s) { return closesocket(s); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <netdb.h>
  using socket_t = int;
  static const socket_t INVALID_SOCKET = -1;
  static int closeSocket(socket_t s) { return ::close(s); }
#endif

namespace hsh {
namespace {

long long epochSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

/// Échappe ce qui doit l'être dans une chaîne JSON. Les valeurs annoncées sont
/// des identifiants, mais un nom d'hôte est choisi par un humain : rien ne
/// garantit qu'il ne contienne pas de guillemet.
std::string jsonEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

}  // namespace

std::string hostName() {
    char buf[256] = {0};
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    if (::gethostname(buf, sizeof(buf) - 1) != 0) return "unknown";
#if defined(_WIN32)
    WSACleanup();
#endif
    std::string name(buf);
    // Un nom pleinement qualifié encombre l'affichage sans rien apporter : le
    // parc vit sur un LAN, où le nom court identifie déjà la machine.
    const auto dot = name.find('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

std::string makeInstanceId(const std::string& app, unsigned short port) {
    // Déterministe : deux démarrages donnent la même valeur, sinon un
    // consommateur croit voir un service disparaître et un autre apparaître.
    std::ostringstream out;
    out << app << "@" << hostName() << ":" << port;
    return out.str();
}

Beacon::Beacon(BeaconIdentity identity, BeaconSettings settings)
    : identity_(std::move(identity)), settings_(settings), startedAt_(epochSeconds()) {}

Beacon::~Beacon() { stop(); }

long long Beacon::uptimeSeconds() const {
    const long long elapsed = epochSeconds() - startedAt_;
    return elapsed < 0 ? 0 : elapsed;   // horloge reculée : jamais négatif
}

std::string Beacon::identityJson(long long uptimeSeconds) const {
    // Construit à la main plutôt qu'avec nlohmann : ce corps est aussi celui du
    // datagramme, où l'ordre et la compacité comptent (limite de 512 octets).
    std::ostringstream out;
    out << "{"
        << "\"proto\":\"morfbeacon/1\""
        << ",\"app\":\"" << jsonEscape(identity_.app) << "\""
        << ",\"host\":\"" << jsonEscape(hostName()) << "\""
        << ",\"version\":\"" << jsonEscape(identity_.version) << "\""
        << ",\"instance\":\"" << jsonEscape(identity_.instance) << "\""
        << ",\"state\":\"ok\""
        << ",\"status_port\":" << identity_.statusPort
        << ",\"uptime_s\":" << uptimeSeconds
        << ",\"ts\":" << epochSeconds()
        << "}";
    return out.str();
}

void Beacon::start() {
    if (!settings_.enabled || running_.exchange(true)) return;
    thread_ = std::thread([this] { loop(); });
}

void Beacon::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
}

void Beacon::loop() {
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    socket_t fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == INVALID_SOCKET) {
        // Pas d'annonce possible : le service continue. Être découvrable est un
        // confort, servir la synchronisation est la mission -- et un hub qui
        // refuserait de démarrer faute de socket UDP échangerait une gêne
        // contre une panne.
        running_ = false;
#if defined(_WIN32)
        WSACleanup();
#endif
        return;
    }

    int broadcast = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                 reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(settings_.udpPort);
    dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    while (running_) {
        const std::string payload = identityJson(uptimeSeconds());
        ::sendto(fd, payload.data(), static_cast<int>(payload.size()), 0,
                 reinterpret_cast<sockaddr*>(&dest), sizeof(dest));

        // Sommeil fractionné : sur un arrêt, le service ne doit pas retenir
        // systemd pendant l'intervalle entier avant de rendre la main.
        const int slice = 200;
        for (int slept = 0; slept < settings_.intervalMs && running_; slept += slice) {
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
        }
    }

    closeSocket(fd);
#if defined(_WIN32)
    WSACleanup();
#endif
}

}  // namespace hsh
