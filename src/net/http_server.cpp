#include "http_server.h"

#include <thread>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cctype>

// --- Abstraction sockets Windows / POSIX -----------------------------------
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  static const socket_t kInvalidSocket = INVALID_SOCKET;
  static int  closeSocket(socket_t s) { return closesocket(s); }
  static int  lastSocketError()       { return WSAGetLastError(); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <errno.h>
  using socket_t = int;
  static const socket_t kInvalidSocket = -1;
  static int  closeSocket(socket_t s) { return ::close(s); }
  static int  lastSocketError()       { return errno; }
#endif

namespace hsh {

// --- Utilitaires de parsing ------------------------------------------------
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Décodage URL (%XX et '+').
std::string urlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex = in.substr(i + 1, 2);
            out.push_back(static_cast<char>(std::strtol(hex.c_str(), nullptr, 16)));
            i += 2;
        } else if (in[i] == '+') {
            out.push_back(' ');
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> out;
    std::string seg;
    std::stringstream ss(path);
    while (std::getline(ss, seg, '/'))
        if (!seg.empty()) out.push_back(seg);
    return out;
}

void parseQuery(const std::string& raw, std::map<std::string, std::string>& query) {
    std::stringstream ss(raw);
    std::string pair;
    while (std::getline(ss, pair, '&')) {
        if (pair.empty()) continue;
        auto eq = pair.find('=');
        if (eq == std::string::npos)
            query[urlDecode(pair)] = "";
        else
            query[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
    }
}

const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "OK";
    }
}

} // namespace

// --- HttpRequest / HttpResponse -------------------------------------------
std::string HttpRequest::header(const std::string& name, const std::string& def) const {
    auto it = headers.find(toLower(name));
    return it == headers.end() ? def : it->second;
}
std::string HttpRequest::queryParam(const std::string& name, const std::string& def) const {
    auto it = query.find(name);
    return it == query.end() ? def : it->second;
}
std::string HttpRequest::param(const std::string& name, const std::string& def) const {
    auto it = params.find(name);
    return it == params.end() ? def : it->second;
}

HttpResponse HttpResponse::json(int status, std::string body) {
    HttpResponse r;
    r.status = status;
    r.body = std::move(body);
    return r;
}
HttpResponse HttpResponse::text(int status, std::string body) {
    HttpResponse r;
    r.status = status;
    r.contentType = "text/plain; charset=utf-8";
    r.body = std::move(body);
    return r;
}

// --- HttpServer ------------------------------------------------------------
HttpServer::HttpServer(std::string host, unsigned short port)
    : host_(std::move(host)), port_(port) {
#if defined(_WIN32)
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

HttpServer::~HttpServer() {
    stop();
#if defined(_WIN32)
    WSACleanup();
#endif
}

void HttpServer::route(const std::string& method, const std::string& pattern, Handler handler) {
    Route r;
    r.method = method;
    r.segments = splitPath(pattern);
    r.handler = std::move(handler);
    routes_.push_back(std::move(r));
}

bool HttpServer::matchRoute(const HttpRequest& req, const Route& route,
                            std::map<std::string, std::string>& outParams) const {
    if (req.method != route.method) return false;
    auto segs = splitPath(req.path);
    if (segs.size() != route.segments.size()) return false;

    std::map<std::string, std::string> captured;
    for (std::size_t i = 0; i < segs.size(); ++i) {
        const std::string& pat = route.segments[i];
        if (!pat.empty() && pat[0] == ':')
            captured[pat.substr(1)] = urlDecode(segs[i]);
        else if (pat != segs[i])
            return false;
    }
    outParams = std::move(captured);
    return true;
}

bool HttpServer::run() {
    listenFd_ = static_cast<long long>(::socket(AF_INET, SOCK_STREAM, 0));
    if (listenFd_ == static_cast<long long>(kInvalidSocket)) return false;

    int yes = 1;
    ::setsockopt(static_cast<socket_t>(listenFd_), SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    if (host_.empty() || host_ == "0.0.0.0")
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        ::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

    if (::bind(static_cast<socket_t>(listenFd_),
               reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closeSocket(static_cast<socket_t>(listenFd_));
        listenFd_ = -1;
        return false;
    }
    if (::listen(static_cast<socket_t>(listenFd_), 16) != 0) {
        closeSocket(static_cast<socket_t>(listenFd_));
        listenFd_ = -1;
        return false;
    }

    running_ = true;
    while (running_) {
        socket_t client = ::accept(static_cast<socket_t>(listenFd_), nullptr, nullptr);
        if (client == kInvalidSocket) {
            if (!running_) break;
            continue;
        }
        std::thread(&HttpServer::handleConnection, this,
                    static_cast<long long>(client)).detach();
    }
    return true;
}

void HttpServer::stop() {
    running_ = false;
    if (listenFd_ != -1) {
        closeSocket(static_cast<socket_t>(listenFd_));
        listenFd_ = -1;
    }
}

void HttpServer::handleConnection(long long clientFd) {
    socket_t fd = static_cast<socket_t>(clientFd);

    // 1) Lire jusqu'à la fin des en-têtes (\r\n\r\n).
    std::string buffer;
    char chunk[4096];
    std::size_t headerEnd = std::string::npos;
    while ((headerEnd = buffer.find("\r\n\r\n")) == std::string::npos) {
        int n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) { closeSocket(fd); return; }
        buffer.append(chunk, static_cast<std::size_t>(n));
        if (buffer.size() > 8 * 1024 * 1024) { closeSocket(fd); return; } // garde-fou en-têtes
    }

    // 2) Parser la ligne de requête + les en-têtes.
    HttpRequest req;
    std::istringstream hs(buffer.substr(0, headerEnd));
    std::string line;
    std::getline(hs, line);
    {
        std::istringstream rl(line);
        std::string target;
        rl >> req.method >> target;
        auto qpos = target.find('?');
        if (qpos == std::string::npos) {
            req.path = urlDecode(target);
        } else {
            req.path = urlDecode(target.substr(0, qpos));
            parseQuery(target.substr(qpos + 1), req.query);
        }
    }
    while (std::getline(hs, line)) {
        line = trim(line);
        if (line.empty()) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        req.headers[toLower(trim(line.substr(0, colon)))] = trim(line.substr(colon + 1));
    }

    // 3) Lire le corps selon Content-Length.
    std::size_t bodyStart = headerEnd + 4;
    std::size_t contentLength = 0;
    if (auto it = req.headers.find("content-length"); it != req.headers.end())
        contentLength = static_cast<std::size_t>(std::strtoul(it->second.c_str(), nullptr, 10));

    req.body = buffer.substr(bodyStart);
    while (req.body.size() < contentLength) {
        int n = ::recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        req.body.append(chunk, static_cast<std::size_t>(n));
    }
    if (req.body.size() > contentLength) req.body.resize(contentLength);

    // 4) Router.
    HttpResponse resp;
    bool matched = false;
    bool pathExists = false;
    for (const auto& r : routes_) {
        std::map<std::string, std::string> params;
        // Un match de chemin mais pas de méthode => 405.
        Route probe = r;
        probe.method = req.method;
        if (matchRoute(req, probe, params)) pathExists = true;

        if (matchRoute(req, r, params)) {
            req.params = std::move(params);
            try {
                resp = r.handler(req);
            } catch (const std::exception& e) {
                resp = HttpResponse::json(500, std::string("{\"error\":\"") + e.what() + "\"}");
            }
            matched = true;
            break;
        }
    }
    if (!matched)
        resp = pathExists ? HttpResponse::json(405, "{\"error\":\"method not allowed\"}")
                          : HttpResponse::json(404, "{\"error\":\"not found\"}");

    // 5) Écrire la réponse.
    std::ostringstream out;
    out << "HTTP/1.1 " << resp.status << ' ' << statusText(resp.status) << "\r\n"
        << "Content-Type: " << resp.contentType << "\r\n"
        << "Content-Length: " << resp.body.size() << "\r\n"
        << "Connection: close\r\n";
    for (const auto& [k, v] : resp.headers)
        out << k << ": " << v << "\r\n";
    out << "\r\n" << resp.body;

    const std::string payload = out.str();
    std::size_t sent = 0;
    while (sent < payload.size()) {
        int n = ::send(fd, payload.data() + sent,
                       static_cast<int>(payload.size() - sent), 0);
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
    closeSocket(fd);
}

} // namespace hsh
