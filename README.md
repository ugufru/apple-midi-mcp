# Apple MIDI MCP

A native C++ MCP (Model Context Protocol) server that provides direct access to Apple's CoreMIDI from any MCP client (e.g., Claude Code). Bidirectional MIDI I/O with SysEx support — send, receive, and interact with hardware synths in real time.

## What makes this different

There are several MIDI MCP servers out there (see [Related Projects](#related-projects)). This one is unique in a few ways:

- **Native C++/CoreMIDI** — no Python runtime, no Node.js, no rtmidi wrapper. Direct framework access with minimal overhead.
- **Bidirectional I/O** — not just send-only. Open a port, buffer incoming MIDI, and read it back. The LLM can *listen* to your instruments.
- **SysEx support** — request and receive SysEx dumps from your synths. Patch management, device identification, parameter editing.
- **stdio transport** — proper MCP over JSON-RPC 2.0. No HTTP server, no WebSocket. Claude Code spawns the binary and talks to it directly.

## Architecture

```
Claude Code  ←─ stdio (JSON-RPC 2.0) ─→  apple-midi-mcp  ←─ CoreMIDI API ─→  MIDI Devices
```

**Components:**
- `src/main.cpp` — Tool registration and entry point
- `src/mcp_server.h/cpp` — Generic MCP server (JSON-RPC 2.0, NDJSON + Content-Length framing, tool dispatch)
- `src/midi_bridge.h/cpp` — CoreMIDI wrapper (enumerate, send, receive, SysEx, buffer)

**Dependencies (minimal):**
- Apple CoreMIDI.framework (ships with macOS)
- [nlohmann/json](https://github.com/nlohmann/json) — fetched automatically by CMake
- CMake 3.20+ and clang++ (Xcode Command Line Tools)

## MCP Tools

| Tool | Description |
|------|-------------|
| `list_devices` | Enumerate all MIDI sources and destinations with names and unique IDs |
| `send_message` | Send MIDI messages (note on/off, CC, SysEx, etc.) to a destination |
| `open_port` | Start listening on a MIDI source, buffering incoming messages |
| `read_buffer` | Read and flush buffered MIDI messages from an open port |
| `close_port` | Stop listening on a MIDI source |

## Building

```bash
make build    # builds apple-midi-mcp binary (CMake)
make test     # runs protocol test suite
make clean    # remove build artifacts
make rebuild  # clean + build
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

Restart Claude Code after adding the config.

## Usage examples

Once installed, Claude Code can interact with your MIDI hardware directly:

- **"List my MIDI devices"** — enumerates all connected interfaces and ports
- **"Send a C major chord to port 1"** — sends note-on messages to a synth
- **"Listen on port 3 and tell me what notes I'm playing"** — opens a port, buffers input, reads it back
- **"Send a SysEx identity request to my Blofeld on port 3"** — queries a synth via SysEx and reads the response
- **"Play a drum pattern on port 10"** — sends sequenced drum hits to a drum machine

## Roadmap

### Phase 1 — Foundation (complete)
- [x] Project setup (repo, README, issue tracking)
- [x] Scaffold project structure (Makefile, src layout)
- [x] JSON-RPC 2.0 / MCP protocol handler over stdio
- [x] CoreMIDI device enumeration
- [x] Main entry point, build, and live test with Claude Code

### Phase 2 — Core MIDI Operations (complete)
- [x] Send MIDI messages (note on/off, CC, program change)
- [x] Receive MIDI messages with FIFO buffering
- [x] SysEx send and receive with reassembly
- [x] Error handling and input validation

### Phase 3 — Robustness & Consistency (complete)
- [x] Generic McpServer class with `add_tool()` registration
- [x] Content-Length framing support (auto-detect NDJSON vs LSP-style)
- [x] CMake build system with FetchContent for dependencies
- [x] Tool exception safety (try/catch around dispatch)
- [x] Fixed protocol version negotiation

### Phase 4 — Extended Features
- [ ] SMF (Standard MIDI File) playback with timing
- [ ] Virtual MIDI port creation
- [ ] Device hot-plug detection

## Related Projects

Other MCP servers with MIDI functionality:

| Project | Language | Description |
|---------|----------|-------------|
| [sandst1/mcp-server-midi](https://github.com/sandst1/mcp-server-midi) | Python | Virtual MIDI port via rtmidi, send-only, HTTP/SSE transport |
| [benjaminr/mcp-koii](https://github.com/benjaminr/mcp-koii) | Python | Teenage Engineering EP-133 K.O. II control via mido |
| [guyko/midimcp](https://github.com/guyko/midimcp) | Kotlin | Guitar pedal control (Meris, Neural DSP, Eventide) with SysEx |
| [necobit/mcp-midi-server](https://github.com/necobit/mcp-midi-server) | Python | Basic MIDI send via python-rtmidi |
| [tubone24/midi-mcp-server](https://github.com/tubone24/midi-mcp-server) | JavaScript | MIDI file generation (no hardware I/O) |
| [xiaolaa2/midi-file-mcp](https://github.com/xiaolaa2/midi-file-mcp) | JavaScript | MIDI file parsing and manipulation |
| [s2d01/daw-midi-generator-mcp](https://github.com/s2d01/daw-midi-generator-mcp) | Python | MIDI file generation for DAW import |

## License

TBD
