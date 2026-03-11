#include "json.hpp"
#include "mcp_handler.h"
#include "midi_bridge.h"
#include <iostream>
#include <string>
#include <csignal>

using json = nlohmann::json;

static volatile bool running = true;

static void signalHandler(int /*sig*/) {
    running = false;
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cerr << "[apple-midi-mcp] Starting..." << std::endl;

    MidiBridge midi;
    McpHandler handler(midi);

    std::string line;
    while (running && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::cerr << "[mcp] << " << line << std::endl;

        json request;
        try {
            request = json::parse(line);
        } catch (const json::parse_error& e) {
            std::cerr << "[mcp] JSON parse error: " << e.what() << std::endl;
            json errorResp = {
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {
                    {"code", -32700},
                    {"message", "Parse error"}
                }}
            };
            std::cout << errorResp.dump() << std::endl;
            continue;
        }

        json response = handler.handleRequest(request);

        // Notifications (no id) don't get a response
        if (response.is_null()) continue;

        std::string out = response.dump();
        std::cerr << "[mcp] >> " << out << std::endl;
        std::cout << out << std::endl;
    }

    std::cerr << "[apple-midi-mcp] Shutting down." << std::endl;
    return 0;
}
