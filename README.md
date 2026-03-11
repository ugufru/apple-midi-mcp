# Apple MIDI MCP

An experimental MCP (Model Context Protocol) server that provides direct access to Apple's CoreMIDI Manager from any MCP client (e.g., Claude Code). Written in C++ for minimal overhead and direct framework access.

## Overview

This MCP server communicates over **stdio** using JSON-RPC 2.0 — no HTTP, no WebSocket, no additional runtime. Claude Code spawns the binary and talks to it directly. The server wraps Apple's CoreMIDI C API to expose MIDI device enumeration, message sending, and message receiving as MCP tools.

## Features

- Enumerate available MIDI interfaces by name and port.
- Send MIDI messages targeted to specific ports.
- Receive MIDI messages from specific ports.
- Active ports log and buffer incoming MIDI data (FIFO — data streamed to the client is removed from the buffer).

## Architecture

```
Claude Code  ←─ stdio (JSON-RPC 2.0) ─→  apple-midi-mcp  ←─ CoreMIDI API ─→  MIDI Devices
```

**Components:**
- `src/main.cpp` — Entry point, stdio read loop
- `src/mcp_handler.h/cpp` — JSON-RPC & MCP protocol handling (initialize, tools/list, tools/call)
- `src/midi_bridge.h/cpp` — CoreMIDI wrapper (enumerate, send, receive, buffer)

**Dependencies (minimal):**
- Apple CoreMIDI.framework (ships with macOS)
- [nlohmann/json](https://github.com/nlohmann/json) — single header-only JSON library
- clang++ (Xcode Command Line Tools)

## MCP Tools Exposed

| Tool | Description |
|------|-------------|
| `list_devices` | Enumerate all MIDI sources and destinations with names and port IDs |
| `send_message` | Send a MIDI message (note on/off, CC, etc.) to a specific destination |
| `open_port` | Start listening on a MIDI source, buffering incoming messages |
| `read_buffer` | Read and flush buffered MIDI messages from an open port |
| `close_port` | Stop listening on a MIDI source |

## Building

```bash
make          # builds apple-midi-mcp binary
make clean    # remove build artifacts
```

## Installation

Add to your Claude Code MCP config (`~/.claude/claude_mcp_settings.json`):

```json
{
  "mcpServers": {
    "apple-midi": {
      "command": "/absolute/path/to/apple-midi-mcp"
    }
  }
}
```

## Roadmap

### Phase 1 — Foundation (current)
- [x] Project setup (repo, README, issue tracking)
- [ ] Scaffold project structure (Makefile, src layout)
- [ ] Implement JSON-RPC 2.0 / MCP protocol handler over stdio
- [ ] Implement CoreMIDI device enumeration
- [ ] Wire up main entry point, build, and test with Claude Code

### Phase 2 — Core MIDI Operations
- [ ] Send MIDI messages (note on/off, CC, program change)
- [ ] Receive MIDI messages with FIFO buffering
- [ ] Port open/close lifecycle management
- [ ] Error handling and validation

### Phase 3 — Extended Features
- [ ] SysEx message support (with safety guardrails)
- [ ] MIDI channel filtering
- [ ] Virtual MIDI port creation
- [ ] Device hot-plug detection

### Phase 4 — Polish
- [ ] Comprehensive logging (stderr)
- [ ] Graceful shutdown handling
- [ ] Performance profiling for low-latency use cases
- [ ] Documentation and usage examples

## License

TBD
