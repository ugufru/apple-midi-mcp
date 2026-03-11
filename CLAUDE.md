# Apple MIDI MCP - Project Guide

## Build & Test
- `make` — builds `apple-midi-mcp` binary
- `make test` — runs 17 protocol tests (shell-based, no framework)
- `make clean` — removes build artifacts
- SDK include path workaround in Makefile for broken CommandLineTools install

## Architecture
- **stdio MCP server** — JSON-RPC 2.0 over stdin/stdout, no HTTP
- `src/main.cpp` — entry point, stdio read loop, signal handling
- `src/mcp_handler.h/cpp` — MCP protocol (initialize, tools/list, tools/call)
- `src/midi_bridge.h/cpp` — CoreMIDI wrapper (enumerate, send, receive, buffer)
- `deps/json.hpp` — nlohmann/json (header-only, checked in)
- Links CoreMIDI.framework and CoreFoundation.framework

## MCP Tools (5 implemented)
- `list_devices` — enumerate MIDI sources/destinations
- `send_message` — send MIDI bytes to a destination by unique ID
- `open_port` — start listening on a source, buffer incoming MIDI
- `read_buffer` — read and flush FIFO buffer from an open port
- `close_port` — stop listening on a source

## Issue Tracking
- All bugs, features, plans tracked in `issues.jsonl` (one JSON object per line)
- Fields: id, title, status (open/in_progress/done), type (task/feature/bug), phase, description
- **Keep issues.jsonl, README.md, and GitHub repo in sync after every change**

## Key Conventions
- Class name is `MidiBridge`, not "MidiManager" (avoid Apple API naming)
- Debug/log output goes to stderr (stdout is reserved for MCP protocol)
- Tests are shell scripts in `test/` — no external test framework
- Minimal dependencies: only nlohmann/json header + macOS system frameworks

## Hardware
- User has a MIDI 16x16 interface (16 source ports, 16 destination ports)

## Current Status
- Phase 1 complete — server builds, runs, registered as Claude Code MCP
- MCP config at `~/.claude/claude_mcp_settings.json`
- Next session: restart Claude Code and test MCP tools live, then Phase 2
