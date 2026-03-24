#include "mcp_server.h"
#include "midi_bridge.h"
#include <csignal>

static void signalHandler(int /*sig*/) {
    // McpServer::run() exits when stdin closes or hits EOF
    // For SIGINT/SIGTERM, we just let the process exit
    std::exit(0);
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    MidiBridge midi;
    McpServer server("apple-midi-mcp", "1.0.0");

    // ── list_devices ─────────────────────────────────────────────────
    server.add_tool(
        "list_devices",
        "Enumerate all available MIDI sources (inputs) and destinations (outputs) with their names and unique IDs.",
        {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
        [&midi](const json& /*args*/) -> json {
            auto devices = midi.listDevices();
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
        });

    // ── send_message ─────────────────────────────────────────────────
    server.add_tool(
        "send_message",
        "Send a MIDI message to a destination. Provide the destination unique ID and message bytes (e.g., [144, 60, 127] for note-on middle C velocity 127 on channel 1).",
        {{"type", "object"},
         {"properties", {
             {"destination_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI destination"}}},
             {"bytes", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "MIDI message bytes (e.g., [144, 60, 127])"}}}
         }},
         {"required", json::array({"destination_id", "bytes"})}},
        [&midi](const json& args) -> json {
            if (!args.contains("destination_id") || !args["destination_id"].is_number_integer()) {
                return {
                    {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'destination_id' (integer required)"}}})},
                    {"isError", true}
                };
            }
            if (!args.contains("bytes") || !args["bytes"].is_array() || args["bytes"].empty()) {
                return {
                    {"content", json::array({{{"type", "text"}, {"text", "Missing or empty 'bytes' array"}}})},
                    {"isError", true}
                };
            }

            int destId = args["destination_id"];
            std::vector<uint8_t> bytes;
            for (const auto& b : args["bytes"]) {
                if (!b.is_number_integer()) {
                    return {
                        {"content", json::array({{{"type", "text"}, {"text", "Each byte must be an integer"}}})},
                        {"isError", true}
                    };
                }
                int val = b.get<int>();
                if (val < 0 || val > 255) {
                    return {
                        {"content", json::array({{{"type", "text"}, {"text", "Byte value out of range (0-255): " + std::to_string(val)}}})},
                        {"isError", true}
                    };
                }
                bytes.push_back(static_cast<uint8_t>(val));
            }

            bool ok = midi.sendMessage(destId, bytes);
            std::string msg = ok ? "Message sent successfully" : "Failed to send message";
            return {
                {"content", json::array({{{"type", "text"}, {"text", msg}}})},
                {"isError", !ok}
            };
        });

    // ── open_port ────────────────────────────────────────────────────
    server.add_tool(
        "open_port",
        "Start listening on a MIDI source. Incoming messages will be buffered until read.",
        {{"type", "object"},
         {"properties", {
             {"source_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI source to listen on"}}}
         }},
         {"required", json::array({"source_id"})}},
        [&midi](const json& args) -> json {
            if (!args.contains("source_id") || !args["source_id"].is_number_integer()) {
                return {
                    {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'source_id' (integer required)"}}})},
                    {"isError", true}
                };
            }
            int sourceId = args["source_id"];
            bool ok = midi.openPort(sourceId);
            std::string msg = ok ? "Port opened, listening for MIDI messages" : "Failed to open port";
            return {
                {"content", json::array({{{"type", "text"}, {"text", msg}}})},
                {"isError", !ok}
            };
        });

    // ── read_buffer ──────────────────────────────────────────────────
    server.add_tool(
        "read_buffer",
        "Read and flush all buffered MIDI messages from an open port. Returns messages as arrays of bytes with timestamps. FIFO: messages are removed after reading.",
        {{"type", "object"},
         {"properties", {
             {"source_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI source to read from"}}}
         }},
         {"required", json::array({"source_id"})}},
        [&midi](const json& args) -> json {
            if (!args.contains("source_id") || !args["source_id"].is_number_integer()) {
                return {
                    {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'source_id' (integer required)"}}})},
                    {"isError", true}
                };
            }
            int sourceId = args["source_id"];
            auto messages = midi.readBuffer(sourceId);

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

            auto stats = midi.getDebugStats();
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
        });

    // ── close_port ───────────────────────────────────────────────────
    server.add_tool(
        "close_port",
        "Stop listening on a MIDI source and discard any remaining buffered messages.",
        {{"type", "object"},
         {"properties", {
             {"source_id", {{"type", "integer"}, {"description", "Unique ID of the MIDI source to stop listening on"}}}
         }},
         {"required", json::array({"source_id"})}},
        [&midi](const json& args) -> json {
            if (!args.contains("source_id") || !args["source_id"].is_number_integer()) {
                return {
                    {"content", json::array({{{"type", "text"}, {"text", "Missing or invalid 'source_id' (integer required)"}}})},
                    {"isError", true}
                };
            }
            int sourceId = args["source_id"];
            bool ok = midi.closePort(sourceId);
            std::string msg = ok ? "Port closed" : "Port was not open";
            return {
                {"content", json::array({{{"type", "text"}, {"text", msg}}})},
                {"isError", !ok}
            };
        });

    server.run();
    return 0;
}
