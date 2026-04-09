#include "http_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <algorithm>

HttpServer::HttpServer() = default;
HttpServer::~HttpServer() { shutdown(); }

bool HttpServer::listen(int port) {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) return false;

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listenFd_); listenFd_ = -1; return false;
    }
    if (::listen(listenFd_, 16) < 0) {
        close(listenFd_); listenFd_ = -1; return false;
    }
    fcntl(listenFd_, F_SETFL, O_NONBLOCK);
    return true;
}

void HttpServer::shutdown() {
    for (auto& c : pending_) close(c.fd);
    pending_.clear();
    if (listenFd_ >= 0) { close(listenFd_); listenFd_ = -1; }
}

void HttpServer::poll(int timeoutMs) {
    std::vector<pollfd> fds;
    fds.push_back({listenFd_, POLLIN, 0});
    for (auto& c : pending_) fds.push_back({c.fd, POLLIN, 0});

    int ret = ::poll(fds.data(), fds.size(), timeoutMs);
    if (ret <= 0) return;

    if (fds[0].revents & POLLIN) acceptNew();

    for (size_t i = 1; i < fds.size(); i++) {
        if (fds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
            int fd = fds[i].fd;
            for (auto& c : pending_) {
                if (c.fd == fd) { handleClient(c); break; }
            }
        }
    }

    // Remove closed fds
    pending_.erase(
        std::remove_if(pending_.begin(), pending_.end(), [](const PendingClient& c) { return c.fd < 0; }),
        pending_.end()
    );
}

void HttpServer::acceptNew() {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int fd = accept(listenFd_, (sockaddr*)&addr, &len);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    pending_.push_back({fd, ""});
}

bool HttpServer::isWebSocketUpgrade(const std::string& request) {
    // Check for Upgrade: websocket header (case-insensitive)
    std::string lower = request;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("upgrade: websocket") != std::string::npos;
}

void HttpServer::handleClient(PendingClient& c) {
    char buf[4096];
    ssize_t n = recv(c.fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        close(c.fd); c.fd = -1; return;
    }
    c.buf.append(buf, n);

    // Wait for full HTTP headers
    if (c.buf.find("\r\n\r\n") == std::string::npos) return;

    // WebSocket upgrade?
    if (isWebSocketUpgrade(c.buf)) {
        if (onUpgrade_) {
            onUpgrade_(c.fd, c.buf);
            c.fd = -1; // Don't close — handed off to WS server
        } else {
            close(c.fd); c.fd = -1;
        }
        return;
    }

    // Parse GET request
    std::string method, path;
    std::istringstream reqLine(c.buf);
    reqLine >> method >> path;

    if (method != "GET") {
        sendResponse(c.fd, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed");
        close(c.fd); c.fd = -1; return;
    }

    // URL decode and sanitize
    path = urlDecode(path);
    // Remove query string
    auto qpos = path.find('?');
    if (qpos != std::string::npos) path = path.substr(0, qpos);
    // Default to index.html
    if (path == "/") path = "/index.html";

    serveFile(c.fd, path);
    close(c.fd);
    c.fd = -1;
}

void HttpServer::serveFile(int fd, const std::string& path) {
    // Security: prevent path traversal
    if (path.find("..") != std::string::npos) {
        sendNotFound(fd); return;
    }

    std::string fullPath = staticDir_ + path;
    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        sendNotFound(fd); return;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string body = ss.str();

    sendResponse(fd, 200, "OK", getMimeType(path), body);
}

void HttpServer::sendResponse(int fd, int code, const std::string& status,
                               const std::string& contentType, const std::string& body) {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << code << " " << status << "\r\n"
         << "Content-Type: " << contentType << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Access-Control-Allow-Origin: *\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << body;
    std::string r = resp.str();
    ::send(fd, r.c_str(), r.size(), 0);
}

void HttpServer::sendNotFound(int fd) {
    sendResponse(fd, 404, "Not Found", "text/plain", "404 Not Found");
}

std::string HttpServer::getMimeType(const std::string& path) {
    if (path.size() >= 5 && path.substr(path.size()-5) == ".html") return "text/html; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size()-4) == ".css")  return "text/css; charset=utf-8";
    if (path.size() >= 3 && path.substr(path.size()-3) == ".js")   return "application/javascript; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size()-4) == ".svg")  return "image/svg+xml";
    if (path.size() >= 4 && path.substr(path.size()-4) == ".png")  return "image/png";
    if (path.size() >= 4 && path.substr(path.size()-4) == ".ico")  return "image/x-icon";
    if (path.size() >= 5 && path.substr(path.size()-5) == ".json") return "application/json";
    return "application/octet-stream";
}

std::string HttpServer::urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int val;
            if (sscanf(s.c_str() + i + 1, "%2x", &val) == 1) {
                out += static_cast<char>(val);
                i += 2;
            } else {
                out += s[i];
            }
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}
