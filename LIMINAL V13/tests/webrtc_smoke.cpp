// V13.3 Machbarkeit: libdatachannel (libjuice, usrsctp, Mbed TLS) mit llvm-mingw - zwei Gegenstellen in einem Prozess,
// direkt und nur ueber das TURN-Relay von libjuice; WebSocket-Server und -Client.
#include <rtc/rtc.hpp>
#include <juice/juice.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

using namespace std::chrono_literals;

static bool waitFor(const std::atomic<bool>& f, int ms) {
    for (int i = 0; i < ms / 10 && !f; ++i) std::this_thread::sleep_for(10ms);
    return f;
}

static bool pair(bool relayOnly, uint16_t turnPort) {
    rtc::Configuration c;
    c.bindAddress = "127.0.0.1";
    if (turnPort) c.iceServers.emplace_back("127.0.0.1", turnPort, "user", "pass", rtc::IceServer::RelayType::TurnUdp);
    if (relayOnly) c.iceTransportPolicy = rtc::TransportPolicy::Relay;
    auto a = std::make_shared<rtc::PeerConnection>(c);
    auto b = std::make_shared<rtc::PeerConnection>(c);
    a->onLocalDescription([b](rtc::Description d) { b->setRemoteDescription(d); });
    a->onLocalCandidate([b](rtc::Candidate x) { b->addRemoteCandidate(x); });
    b->onLocalDescription([a](rtc::Description d) { a->setRemoteDescription(d); });
    b->onLocalCandidate([a](rtc::Candidate x) { a->addRemoteCandidate(x); });
    std::atomic<bool> got{false}, open{false};
    std::shared_ptr<rtc::DataChannel> bdc;
    b->onDataChannel([&](std::shared_ptr<rtc::DataChannel> dc) {
        bdc = dc;
        dc->onMessage([&](rtc::message_variant m) {
            if (std::holds_alternative<rtc::binary>(m) && std::get<rtc::binary>(m).size() == 1200) got = true;
        });
    });
    auto dc = a->createDataChannel("r");
    dc->onOpen([&] { open = true; });
    const auto t0 = std::chrono::steady_clock::now();
    if (!waitFor(open, 10000)) {
        std::printf("  Kanal nicht offen (relay=%d)\n", relayOnly);
        return false;
    }
    dc->send(rtc::binary(1200, std::byte{7}));
    const bool ok = waitFor(got, 5000);
    rtc::Candidate l, r;
    std::string path = "?";
    static const char* tn[] = {"?", "host", "srflx", "prflx", "relay"};
    if (a->getSelectedCandidatePair(&l, &r)) path = std::string(tn[(int)l.type()]) + " -> " + tn[(int)r.type()];
    std::printf("  relayOnly=%d: offen nach %lld ms, Weg %s, Fingerabdruck geprueft: %s, 1200 Byte %s\n", relayOnly,
                (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count(),
                path.c_str(), a->remoteFingerprint().value.empty() ? "nein" : "ja", ok ? "angekommen" : "FEHLEN");
    a->close();
    b->close();
    return ok;
}

int main() {
    rtc::InitLogger(rtc::LogLevel::Warning);
    int fails = 0;
    fails += pair(false, 0) ? 0 : 1;
    // TURN-Server von libjuice
    juice_server_credentials_t cred{"user", "pass", 8};
    juice_server_config_t sc{};
    sc.credentials = &cred;
    sc.credentials_count = 1;
    sc.max_allocations = 16;
    sc.bind_address = "127.0.0.1";
    sc.port = 0;
    sc.realm = "liminal";
    juice_server_t* srv = juice_server_create(&sc);
    if (!srv) {
        std::printf("  TURN-Server startet nicht\n");
        ++fails;
    } else {
        std::printf("  TURN-Server auf Port %u\n", juice_server_get_port(srv));
        fails += pair(true, juice_server_get_port(srv)) ? 0 : 1;
    }
    // WebSocket
    {
        rtc::WebSocketServer::Configuration wc;
        wc.port = 0;
        wc.bindAddress = "127.0.0.1";
        rtc::WebSocketServer server(wc);
        std::shared_ptr<rtc::WebSocket> peer;
        server.onClient([&](std::shared_ptr<rtc::WebSocket> ws) {
            peer = ws;
            ws->onMessage([ws](rtc::message_variant m) {
                if (std::holds_alternative<std::string>(m)) ws->send("echo:" + std::get<std::string>(m));
            });
        });
        rtc::WebSocket client;
        std::atomic<bool> open{false}, echo{false};
        client.onOpen([&] { open = true; });
        client.onMessage([&](rtc::message_variant m) {
            if (std::holds_alternative<std::string>(m) && std::get<std::string>(m) == "echo:hallo") echo = true;
        });
        client.open("ws://127.0.0.1:" + std::to_string(server.port()) + "/lobby");
        if (waitFor(open, 5000)) client.send("hallo");
        const bool ok = waitFor(echo, 5000);
        std::printf("  WebSocket: %s\n", ok ? "Echo angekommen" : "FEHLER");
        fails += ok ? 0 : 1;
        client.close();
        server.stop();
    }
    if (srv) juice_server_destroy(srv);
    std::printf(fails ? "FEHLER: %d\n" : "OK\n", fails);
    return fails ? 1 : 0;
}
