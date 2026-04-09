#pragma once
#include <string>
#include <vector>
#include <functional>

// Minimal HTTP server for serving static files from a directory.
// Runs alongside the WebSocket server on the same port — non-WS requests
// get served static files; WS upgrade requests are forwarded.

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    bool listen(int port);
    void setStaticDir(const std::string& dir) { staticDir_ = dir; }

    // Poll for new HTTP connections (non-blocking).
    // Returns a new client fd if it's a WebSocket upgrade, -1 otherwise.
    void poll(int timeoutMs);

    // Check if a raw HTTP request in the buffer is a WebSocket upgrade.
    // Used by main to route between HTTP and WS.
    static bool isWebSocketUpgrade(const std::string& request);

    void shutdown();

    int listenFd() const { return listenFd_; }

    // Callback for WS upgrades — hands the fd + buffered data to the WS server
    using UpgradeCallback = std::function<void(int fd, const std::string& data)>;
    void setOnUpgrade(UpgradeCallback cb) { onUpgrade_ = cb; }

private:
    struct PendingClient {
        int fd;
        std::string buf;
    };

    int listenFd_ = -1;
    std::string staticDir_;
    std::vector<PendingClient> pending_;
    UpgradeCallback onUpgrade_;

    void acceptNew();
    void handleClient(PendingClient& c);
    void serveFile(int fd, const std::string& path);
    void sendResponse(int fd, int code, const std::string& status,
                      const std::string& contentType, const std::string& body);
    void sendNotFound(int fd);

    static std::string getMimeType(const std::string& path);
    static std::string urlDecode(const std::string& s);
};
