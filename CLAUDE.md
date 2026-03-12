# Apple MIDI MCP - Project Guide

## Build & Test
- `make` — builds `apple-midi-mcp` binary
- `make test` — runs protocol tests (shell-based, no framework)
- `make clean` — removes build artifacts

## Conventions
- Class name is `MidiBridge`, not "MidiManager" (avoid Apple API naming)
- Debug/log output goes to stderr (stdout is reserved for MCP protocol)
- Tests are shell scripts in `test/` — no external test framework
- Minimal dependencies: only nlohmann/json header + macOS system frameworks

## Issue Tracking
- All bugs, features, plans tracked in `issues.jsonl` (one JSON object per line)
- Fields: id, title, status (open/in_progress/done), type (task/feature/bug), phase, description
- Keep issues.jsonl, README.md, and GitHub repo in sync after changes
