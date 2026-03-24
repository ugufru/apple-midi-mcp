#include "mcp_server.h"

#include <iostream>
#include <stdexcept>

McpServer::McpServer(const std::string& name, const std::string& version)
    : name_(name), version_(version) {}

void McpServer::add_tool(const std::string& name,
                         const std::string& description,
                         const json& input_schema, ToolHandler handler) {
    tools_[name] = {name, description, input_schema, std::move(handler)};
}

json McpServer::read_message() {
    // Support both Content-Length framing (LSP-style) and bare NDJSON.
    // Detect mode by peeking at the first non-whitespace byte:
    //   '{' → NDJSON line
    //   'C'  → Content-Length header

    // Skip blank lines between messages
    int ch;
    while ((ch = std::cin.peek()) != EOF) {
        if (ch != '\n' && ch != '\r') break;
        std::cin.get();
    }
    if (ch == EOF) {
        throw std::runtime_error("EOF");
    }

    if (ch == '{') {
        // NDJSON mode: read one line of JSON
        ndjson_mode_ = true;
        std::string line;
        if (!std::getline(std::cin, line)) {
            throw std::runtime_error("EOF reading NDJSON line");
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return json::parse(line);
    }

    if (ch != 'C') {
        // Not JSON and not Content-Length — read the line and attempt to parse it
        // (will throw json::parse_error which the run loop catches)
        ndjson_mode_ = true;
        std::string line;
        if (!std::getline(std::cin, line)) {
            throw std::runtime_error("EOF");
        }
        return json::parse(line);  // will throw json::parse_error
    }

    // Content-Length framing mode
    std::string line;
    int content_length = -1;

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        if (line.rfind("Content-Length:", 0) == 0) {
            content_length = std::stoi(line.substr(15));
        }
    }

    if (content_length < 0) {
        throw std::runtime_error("No Content-Length header");
    }

    std::string body(content_length, '\0');
    std::cin.read(body.data(), content_length);

    if (std::cin.gcount() != content_length) {
        throw std::runtime_error("Incomplete message body");
    }

    return json::parse(body);
}

void McpServer::write_message(const json& msg) {
    std::string body = msg.dump();
    if (ndjson_mode_) {
        std::cout << body << "\n";
    } else {
        std::cout << "Content-Length: " << body.size() << "\r\n"
                  << "\r\n"
                  << body;
    }
    std::cout.flush();
}

json McpServer::make_response(const json& id, const json& result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
}

json McpServer::make_error(const json& id, int code,
                           const std::string& message) {
    return {{"jsonrpc", "2.0"},
            {"id", id},
            {"error", {{"code", code}, {"message", message}}}};
}

void McpServer::handle(const json& msg) {
    std::string method = msg.value("method", "");
    json id = msg.contains("id") ? msg["id"] : json(nullptr);

    std::cerr << "[mcp] Received method: " << method << std::endl;

    // ── initialize ───────────────────────────────────────────────────
    if (method == "initialize") {
        json result = {
            {"protocolVersion", "2024-11-05"},
            {"capabilities",
             {{"tools", json::object()}}},
            {"serverInfo", {{"name", name_}, {"version", version_}}}};
        write_message(make_response(id, result));
        return;
    }

    // ── notifications (no response needed) ───────────────────────────
    if (id.is_null()) {
        return;
    }

    // ── ping ─────────────────────────────────────────────────────────
    if (method == "ping") {
        write_message(make_response(id, json::object()));
        return;
    }

    // ── tools/list ───────────────────────────────────────────────────
    if (method == "tools/list") {
        json tool_list = json::array();
        for (auto& [name, tool] : tools_) {
            tool_list.push_back(
                {{"name", tool.name},
                 {"description", tool.description},
                 {"inputSchema", tool.input_schema}});
        }
        write_message(make_response(id, {{"tools", tool_list}}));
        return;
    }

    // ── tools/call ───────────────────────────────────────────────────
    if (method == "tools/call") {
        if (!msg.contains("params") || !msg["params"].contains("name")) {
            write_message(make_error(id, -32602, "Missing params.name in tools/call"));
            return;
        }

        std::string tool_name = msg["params"]["name"];
        json arguments = msg["params"].value("arguments", json::object());

        std::cerr << "[mcp] Tool call: " << tool_name << std::endl;

        auto it = tools_.find(tool_name);
        if (it == tools_.end()) {
            write_message(
                make_error(id, -32602, "Unknown tool: " + tool_name));
            return;
        }

        try {
            json content = it->second.handler(arguments);
            write_message(make_response(id, content));
        } catch (const std::exception& e) {
            json error_content = json::array();
            error_content.push_back(
                {{"type", "text"}, {"text", std::string(e.what())}});
            write_message(
                make_response(id, {{"content", error_content},
                                   {"isError", true}}));
        }
        return;
    }

    // ── unknown method ───────────────────────────────────────────────
    write_message(make_error(id, -32601, "Method not found: " + method));
}

void McpServer::run() {
    std::cerr << name_ << ": server starting" << std::endl;

    while (std::cin.good()) {
        try {
            json msg = read_message();
            handle(msg);
        } catch (const json::parse_error& e) {
            // JSON parse error — send proper JSON-RPC error
            json errorResp = {
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {
                    {"code", -32700},
                    {"message", "Parse error"}
                }}
            };
            write_message(errorResp);
        } catch (const std::exception& e) {
            // EOF or other fatal error — exit gracefully
            std::cerr << name_ << ": " << e.what() << std::endl;
            break;
        }
    }

    std::cerr << name_ << ": shutting down" << std::endl;
}
