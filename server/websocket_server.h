#pragma once
#include <string>
#include <vector>
#include <functional>

// Minimal WebSocket server — no external dependencies.
// Implements RFC 6455 basics: handshake, text frames, ping/pong, close.

class WebSocketServer {
public:
    using MessageCallback = std::function<void(int clientFd, const std::string& msg)>;
    using ConnectCallback = std::function<void(int clientFd)>;
    using DisconnectCallback = std::function<void(int clientFd)>;

    WebSocketServer();
    ~WebSocketServer();

    bool listen(int port);
    void setOnMessage(MessageCallback cb)       { onMessage_ = cb; }
    void setOnConnect(ConnectCallback cb)        { onConnect_ = cb; }
    void setOnDisconnect(DisconnectCallback cb)  { onDisconnect_ = cb; }

    // Poll for events (non-blocking, timeout in ms)
    void poll(int timeoutMs);

    // Send text frame to a specific client
    void send(int clientFd, const std::string& data);

    // Send to all connected WS clients
    void broadcast(const std::string& data);

    // Close
    void shutdown();

    bool hasClients() const { return !clients_.empty(); }

private:
    struct Client {
        int fd;
        bool wsReady;          // completed handshake
        std::string recvBuf;   // incoming data buffer
        bool httpRequest;      // still in HTTP phase
    };

    int listenFd_ = -1;
    std::vector<Client> clients_;

    MessageCallback onMessage_;
    ConnectCallback onConnect_;
    DisconnectCallback onDisconnect_;

    void acceptNewClient();
    void handleClientData(Client& c);
    void doHandshake(Client& c);
    void processWebSocketFrame(Client& c);
    void sendFrame(int fd, int opcode, const std::string& payload);
    void removeClient(int fd);

    static std::string computeAcceptKey(const std::string& clientKey);
    static std::string base64Encode(const unsigned char* data, size_t len);
    static void sha1(const std::string& input, unsigned char output[20]);
};
