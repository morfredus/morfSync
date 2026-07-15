/**
 * http_server.h — Serveur HTTP/1.1 minimal, sans dépendance externe.
 *
 * Volontairement minuscule : HomeServerHub est un hub LAN privé dont on écrit
 * aussi les clients. On n'a besoin ni de TLS, ni de keep-alive, ni de HTTP/2.
 * Une connexion = une requête = une réponse (Connection: close). Un thread par
 * connexion. Portable Windows (winsock2) / Linux / ARM64 (sockets POSIX).
 *
 * Ce fichier ne connaît RIEN de la synchro : c'est un routeur générique.
 */

#pragma once
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <atomic>

namespace hsh {

struct HttpRequest {
    std::string method;                         // "GET", "POST"…
    std::string path;                           // "/api/componenthub/changes"
    std::map<std::string, std::string> query;   // ?since=10&limit=50
    std::map<std::string, std::string> headers; // clés en minuscules
    std::map<std::string, std::string> params;  // segments capturés (:domain…)
    std::string body;

    std::string header(const std::string& name, const std::string& def = "") const;
    std::string queryParam(const std::string& name, const std::string& def = "") const;
    std::string param(const std::string& name, const std::string& def = "") const;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
    std::map<std::string, std::string> headers;

    // Fabrique une réponse JSON à partir d'un corps déjà sérialisé.
    static HttpResponse json(int status, std::string body);
    static HttpResponse text(int status, std::string body);
};

using Handler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
public:
    HttpServer(std::string host, unsigned short port);
    ~HttpServer();

    // Enregistre une route. Le motif accepte un segment paramétré ":nom"
    // (ex. "/api/:domain/changes"), capturé dans req.params["nom"].
    void route(const std::string& method, const std::string& pattern, Handler handler);

    // Boucle d'acceptation bloquante. Retourne false si le socket d'écoute
    // n'a pas pu être ouvert (port occupé, permission…).
    bool run();
    void stop();

private:
    struct Route {
        std::string method;
        std::vector<std::string> segments; // ":x" = paramètre
        Handler handler;
    };

    void handleConnection(long long clientFd);
    bool matchRoute(const HttpRequest& req, const Route& route,
                    std::map<std::string, std::string>& outParams) const;

    std::string host_;
    unsigned short port_;
    long long listenFd_ = -1;   // SOCKET (Windows) ou int (POSIX), stocké en 64 bits
    std::vector<Route> routes_;
    std::atomic<bool> running_{false};
};

} // namespace hsh
