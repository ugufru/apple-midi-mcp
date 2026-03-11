#pragma once

#include "json.hpp"
#include "midi_bridge.h"
#include <string>

using json = nlohmann::json;

class McpHandler {
public:
    McpHandler(MidiBridge& midi);

    // Process a JSON-RPC request and return a JSON-RPC response
    json handleRequest(const json& request);

private:
    MidiBridge& midi_;

    json handleInitialize(const json& request);
    json handleToolsList(const json& request);
    json handleToolsCall(const json& request);
    json handlePing(const json& request);

    // Tool implementations
    json toolListDevices(const json& args);
    json toolSendMessage(const json& args);
    json toolOpenPort(const json& args);
    json toolReadBuffer(const json& args);
    json toolClosePort(const json& args);

    // Helpers
    json makeResponse(const json& id, const json& result);
    json makeError(const json& id, int code, const std::string& message);
};
