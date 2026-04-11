#include "websocket_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <sstream>

// ── SHA-1 (RFC 3174) — minimal implementation ──────

static uint32_t sha1_rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

void WebSocketServer::sha1(const std::string& input, unsigned char output[20]) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

    size_t len = input.size();
    size_t bitLen = len * 8;
    // Pad
    std::vector<uint8_t> msg(input.begin(), input.end());
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) msg.push_back(0);
    for (int i = 7; i >= 0; i--) msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (msg[chunk+i*4] << 24) | (msg[chunk+i*4+1] << 16) | (msg[chunk+i*4+2] << 8) | msg[chunk+i*4+3];
        for (int i = 16; i < 80; i++)
            w[i] = sha1_rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else              { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
            uint32_t temp = sha1_rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = sha1_rotl(b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    uint32_t h[] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; i++) {
        output[i*4]   = (h[i] >> 24) & 0xFF;
        output[i*4+1] = (h[i] >> 16) & 0xFF;
        output[i*4+2] = (h[i] >> 8)  & 0xFF;
        output[i*4+3] = h[i] & 0xFF;
    }
}

// ── Base64 ──────────────────────────────────────────

std::string WebSocketServer::base64Encode(const unsigned char* data, size_t len) {
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i+1 < len) n |= static_cast<uint32_t>(data[i+1]) << 8;
        if (i+2 < len) n |= data[i+2];
        out += t[(n >> 18) & 0x3F];
        out += t[(n >> 12) & 0x3F];
        out += (i+1 < len) ? t[(n >> 6) & 0x3F] : '=';
        out += (i+2 < len) ? t[n & 0x3F] : '=';
    }
    return out;
}

std::string WebSocketServer::computeAcceptKey(const std::string& clientKey) {
    std::string concat = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char hash[20];
    sha1(concat, hash);
    return base64Encode(hash, 20);
}

// ── Server lifecycle ────────────────────────────────

WebSocketServer::WebSocketServer() = default;

WebSocketServer::~WebSocketServer() { shutdown(); }

bool WebSocketServer::listen(int port) {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) return false;

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listenFd_); listenFd_ = -1; return false;
    }
    if (::listen(listenFd_, 8) < 0) {
        close(listenFd_); listenFd_ = -1; return false;
    }

    // Non-blocking
    fcntl(listenFd_, F_SETFL, O_NONBLOCK);
    return true;
}

void WebSocketServer::shutdown() {
    for (auto& c : clients_) close(c.fd);
    clients_.clear();
    if (listenFd_ >= 0) { close(listenFd_); listenFd_ = -1; }
}

// ── Poll ────────────────────────────────────────────

void WebSocketServer::poll(int timeoutMs) {
    std::vector<pollfd> fds;
    fds.push_back({listenFd_, POLLIN, 0});
    for (auto& c : clients_) fds.push_back({c.fd, POLLIN, 0});

    int ret = ::poll(fds.data(), fds.size(), timeoutMs);
    if (ret <= 0) return;

    // New connection?
    if (fds[0].revents & POLLIN) acceptNewClient();

    // Client data
    for (size_t i = 1; i < fds.size(); i++) {
        if (fds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
            // Find client
            int fd = fds[i].fd;
            for (auto& c : clients_) {
                if (c.fd == fd) {
                    handleClientData(c);
                    break;
                }
            }
        }
    }
}

void WebSocketServer::acceptNewClient() {
    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    int fd = accept(listenFd_, (sockaddr*)&addr, &len);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    clients_.push_back({fd, false, "", true});
}

void WebSocketServer::handleClientData(Client& c) {
    char buf[4096];
    ssize_t n = recv(c.fd, buf, sizeof(buf), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;  // no data yet, not a disconnect
        if (c.wsReady && onDisconnect_) onDisconnect_(c.fd);
        removeClient(c.fd);
        return;
    }
    if (n == 0) {  // peer closed connection
        if (c.wsReady && onDisconnect_) onDisconnect_(c.fd);
        removeClient(c.fd);
        return;
    }
    c.recvBuf.append(buf, n);

    if (!c.wsReady) {
        doHandshake(c);
    } else {
        processWebSocketFrame(c);
    }
}

// ── WebSocket handshake ─────────────────────────────

void WebSocketServer::doHandshake(Client& c) {
    // Wait for complete HTTP request
    if (c.recvBuf.find("\r\n\r\n") == std::string::npos) return;

    // Extract Sec-WebSocket-Key
    std::string key;
    std::istringstream stream(c.recvBuf);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("Sec-WebSocket-Key:") != std::string::npos) {
            size_t pos = line.find(':');
            key = line.substr(pos + 1);
            // Trim
            while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
            while (!key.empty() && (key.back() == '\r' || key.back() == '\n' || key.back() == ' ')) key.pop_back();
            break;
        }
    }

    if (key.empty()) {
        // Not a WebSocket upgrade — close
        removeClient(c.fd);
        return;
    }

    std::string accept = computeAcceptKey(key);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "\r\n";

    ::send(c.fd, response.c_str(), response.size(), 0);
    c.wsReady = true;
    c.recvBuf.clear();

    if (onConnect_) onConnect_(c.fd);
}

// ── WebSocket frame processing ──────────────────────

void WebSocketServer::processWebSocketFrame(Client& c) {
    while (c.recvBuf.size() >= 2) {
        uint8_t b0 = static_cast<uint8_t>(c.recvBuf[0]);
        uint8_t b1 = static_cast<uint8_t>(c.recvBuf[1]);

        int opcode = b0 & 0x0F;
        bool masked = (b1 & 0x80) != 0;
        uint64_t payloadLen = b1 & 0x7F;

        size_t headerLen = 2;
        if (payloadLen == 126) {
            if (c.recvBuf.size() < 4) return;
            payloadLen = (static_cast<uint8_t>(c.recvBuf[2]) << 8) | static_cast<uint8_t>(c.recvBuf[3]);
            headerLen = 4;
        } else if (payloadLen == 127) {
            if (c.recvBuf.size() < 10) return;
            payloadLen = 0;
            for (int i = 0; i < 8; i++)
                payloadLen = (payloadLen << 8) | static_cast<uint8_t>(c.recvBuf[2 + i]);
            headerLen = 10;
        }

        size_t maskLen = masked ? 4 : 0;
        size_t totalLen = headerLen + maskLen + payloadLen;
        if (c.recvBuf.size() < totalLen) return;

        // Extract mask and payload
        uint8_t mask[4] = {0};
        if (masked) {
            for (int i = 0; i < 4; i++)
                mask[i] = static_cast<uint8_t>(c.recvBuf[headerLen + i]);
        }

        std::string payload(payloadLen, '\0');
        for (uint64_t i = 0; i < payloadLen; i++) {
            payload[i] = c.recvBuf[headerLen + maskLen + i] ^ (masked ? mask[i % 4] : 0);
        }

        c.recvBuf.erase(0, totalLen);

        switch (opcode) {
            case 0x1: // Text
                if (onMessage_) onMessage_(c.fd, payload);
                break;
            case 0x8: // Close
                if (onDisconnect_) onDisconnect_(c.fd);
                removeClient(c.fd);
                return;
            case 0x9: // Ping → Pong
                sendFrame(c.fd, 0xA, payload);
                break;
            case 0xA: // Pong — ignore
                break;
        }
    }
}

// ── Send frame ──────────────────────────────────────

void WebSocketServer::sendFrame(int fd, int opcode, const std::string& payload) {
    std::vector<uint8_t> frame;
    frame.push_back(0x80 | (opcode & 0x0F)); // FIN + opcode

    if (payload.size() < 126) {
        frame.push_back(static_cast<uint8_t>(payload.size()));
    } else if (payload.size() < 65536) {
        frame.push_back(126);
        frame.push_back((payload.size() >> 8) & 0xFF);
        frame.push_back(payload.size() & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--)
            frame.push_back((payload.size() >> (i * 8)) & 0xFF);
    }

    frame.insert(frame.end(), payload.begin(), payload.end());
    ::send(fd, frame.data(), frame.size(), MSG_NOSIGNAL);
}

void WebSocketServer::send(int clientFd, const std::string& data) {
    sendFrame(clientFd, 0x1, data);
}

void WebSocketServer::broadcast(const std::string& data) {
    for (auto& c : clients_) {
        if (c.wsReady) sendFrame(c.fd, 0x1, data);
    }
}

void WebSocketServer::removeClient(int fd) {
    close(fd);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(), [fd](const Client& c) { return c.fd == fd; }),
        clients_.end()
    );
}
