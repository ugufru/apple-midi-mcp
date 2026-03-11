#include "mcp_handler.h"
#include <iostream>

McpHandler::McpHandler(MidiBridge& midi) : midi_(midi) {}

json McpHandler::handleRequest(const json& request) {
    std::string method = request.value("method", "");
    json id = request.contains("id") ? request["id"] : json(nullptr);

    std::cerr << "[mcp] Received method: " << method << std::endl;

    if (method == "initialize") {
        return handleInitialize(request);
    } else if (method == "notifications/initialized") {
        // Client acknowledgement, no response needed
        return json(nullptr);
    } else if (method == "ping") {
        return handlePing(request);
    } else if (method == "tools/list") {
        return handleToolsList(request);
    } else if (method == "tools/call") {
        return handleToolsCall(request);
    } else {
        return makeError(id, -32601, "Method not found: " + method);
    }
}

json McpHandler::handleInitialize(const json& request) {
    json id = request["id"];

    // Echo back the client's requested protocol version if we support it
    std::string clientVersion = "2024-11-05";
    if (request.contains("params") && request["params"].contains("protocolVersion")) {
        clientVersion = request["params"]["protocolVersion"];
    }

    return makeResponse(id, {
        {"protocolVersion", clientVersion},
        {"capabilities", {
            {"tools", json::object()}
        }},
        {"serverInfo", {
            {"name", "apple-midi-mcp"},
            {"version", "0.1.0"}
        }}
    });
}

json McpHandler::handlePing(const json& request) {
    return makeResponse(request["id"], json::object());
}

json McpHandler::handleToolsList(const json& request) {
    json tools = json::array();

    tools.push_back({
        {"name", "list_devices"},
        {"description", "Enumerate all available MIDI sources (inputs) and destinations (outputs) with their names and unique IDs."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"required", json::array()}
        }}
    });

    tools.push_back({
        {"name", "send_message"},
        {"description", "Send a MIDI message to a destination. Provide the destination unique ID and message bytes (e.g., [144, 60, 127] for note-on middle C velocity 127 on channel 1)."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"destination_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI destination"}}},
                {"bytes", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "MIDI message bytes (e.g., [144, 60, 127])"}}}
            }},
            {"required", json::array({"destination_id", "bytes"})}
        }}
    });

    tools.push_back({
        {"name", "open_port"},
        {"description", "Start listening on a MIDI source. Incoming messages will be buffered until read."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"source_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI source to listen on"}}}
            }},
            {"required", json::array({"source_id"})}
        }}
    });

    tools.push_back({
        {"name", "read_buffer"},
        {"description", "Read and flush all buffered MIDI messages from an open port. Returns messages as arrays of bytes with timestamps. FIFO: messages are removed after reading."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"source_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI source to read from"}}}
            }},
            {"required", json::array({"source_id"})}
        }}
    });

    tools.push_back({
        {"name", "close_port"},
        {"description", "Stop listening on a MIDI source and discard any remaining buffered messages."},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"source_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI source to stop listening on"}}}
            }},
            {"required", json::array({"source_id"})}
        }}
    });

    return makeResponse(request["id"], {{"tools", tools}});
}

json McpHandler::handleToolsCall(const json& request) {
    json id = request["id"];
    std::string toolName = request["params"]["name"];
    json args = request["params"].value("arguments", json::object());

    std::cerr << "[mcp] Tool call: " << toolName << std::endl;

    json result;
    if (toolName == "list_devices") {
        result = toolListDevices(args);
    } else if (toolName == "send_message") {
        result = toolSendMessage(args);
    } else if (toolName == "open_port") {
        result = toolOpenPort(args);
    } else if (toolName == "read_buffer") {
        result = toolReadBuffer(args);
    } else if (toolName == "close_port") {
        result = toolClosePort(args);
    } else {
        return makeError(id, -32602, "Unknown tool: " + toolName);
    }

    return makeResponse(id, result);
}

json McpHandler::toolListDevices(const json& /*args*/) {
    auto devices = midi_.listDevices();
    json devArray = json::array();

    for (const auto& dev : devices) {
        devArray.push_back({
            {"name", dev.name},
            {"unique_id", dev.uniqueId},
            {"type", dev.isSource ? "source" : "destination"}
        });
    }

    return {
        {"content", json::array({
            {{"type", "text"}, {"text", devArray.dump(2)}}
        })}
    };
}

json McpHandler::toolSendMessage(const json& args) {
    int destId = args["destination_id"];
    std::vector<uint8_t> bytes;
    for (const auto& b : args["bytes"]) {
        bytes.push_back(static_cast<uint8_t>(b.get<int>()));
    }

    bool ok = midi_.sendMessage(destId, bytes);
    std::string msg = ok ? "Message sent successfully" : "Failed to send message";

    return {
        {"content", json::array({
            {{"type", "text"}, {"text", msg}}
        })},
        {"isError", !ok}
    };
}

json McpHandler::toolOpenPort(const json& args) {
    int sourceId = args["source_id"];
    bool ok = midi_.openPort(sourceId);
    std::string msg = ok ? "Port opened, listening for MIDI messages" : "Failed to open port";

    return {
        {"content", json::array({
            {{"type", "text"}, {"text", msg}}
        })},
        {"isError", !ok}
    };
}

json McpHandler::toolReadBuffer(const json& args) {
    int sourceId = args["source_id"];
    auto messages = midi_.readBuffer(sourceId);

    json msgArray = json::array();
    for (const auto& msg : messages) {
        json bytesArray = json::array();
        for (uint8_t b : msg.bytes) {
            bytesArray.push_back(b);
        }
        msgArray.push_back({
            {"bytes", bytesArray},
            {"timestamp", msg.timestamp}
        });
    }

    auto stats = midi_.getDebugStats();
    json debugInfo = {
        {"callbacks", stats.callbacks},
        {"packets", stats.packets},
        {"sysex_starts", stats.sysexStarts},
        {"sysex_complete", stats.sysexComplete},
        {"regular_msgs", stats.regularMsgs}
    };

    json result = {
        {"messages", msgArray},
        {"debug", debugInfo}
    };

    return {
        {"content", json::array({
            {{"type", "text"}, {"text", result.dump(2)}}
        })}
    };
}

json McpHandler::toolClosePort(const json& args) {
    int sourceId = args["source_id"];
    bool ok = midi_.closePort(sourceId);
    std::string msg = ok ? "Port closed" : "Port was not open";

    return {
        {"content", json::array({
            {{"type", "text"}, {"text", msg}}
        })},
        {"isError", !ok}
    };
}

json McpHandler::makeResponse(const json& id, const json& result) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
}

json McpHandler::makeError(const json& id, int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"error", {
            {"code", code},
            {"message", message}
        }}
    };
}
