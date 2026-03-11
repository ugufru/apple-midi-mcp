#!/bin/bash
# Protocol-level smoke tests for apple-midi-mcp
# Tests the MCP JSON-RPC interface via stdin/stdout

BINARY="${1:-./apple-midi-mcp}"
PASS=0
FAIL=0

check() {
    local name="$1"
    local input="$2"
    local expected="$3"

    local output
    output=$(echo "$input" | "$BINARY" 2>/dev/null) || true

    if echo "$output" | grep -q "$expected"; then
        echo "  PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name"
        echo "    expected to contain: $expected"
        echo "    got: $output"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== apple-midi-mcp protocol tests ==="
echo ""

# Test 1: Initialize handshake
check "initialize returns protocolVersion" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"0.1"}}}' \
    '"protocolVersion":"2024-11-05"'

# Test 2: Initialize returns server info
check "initialize returns serverInfo" \
    '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"0.1"}}}' \
    '"name":"apple-midi-mcp"'

# Test 3: tools/list returns tools array
check "tools/list returns tools" \
    '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' \
    '"name":"list_devices"'

# Test 4: tools/list includes all 5 tools
for tool in list_devices send_message open_port read_buffer close_port; do
    check "tools/list includes $tool" \
        '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}' \
        "\"name\":\"$tool\""
done

# Test 5: list_devices returns content
check "list_devices returns content array" \
    '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"list_devices","arguments":{}}}' \
    '"type":"text"'

# Test 6: Unknown method returns error
check "unknown method returns error" \
    '{"jsonrpc":"2.0","id":1,"method":"foo/bar","params":{}}' \
    '"error"'

# Test 7: Unknown tool returns error
check "unknown tool returns error" \
    '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"nonexistent","arguments":{}}}' \
    '"error"'

# Test 8: Invalid JSON returns parse error
check "invalid JSON returns parse error" \
    'not json at all' \
    '"code":-32700'

# Test 9: Ping
check "ping returns empty result" \
    '{"jsonrpc":"2.0","id":99,"method":"ping","params":{}}' \
    '"id":99'

# Test 10: send_message to invalid destination returns error
check "send_message to invalid dest fails gracefully" \
    '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"send_message","arguments":{"destination_id":999999,"bytes":[144,60,127]}}}' \
    '"isError":true'

# Test 11: open_port with invalid source fails gracefully
check "open_port with invalid source fails gracefully" \
    '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"open_port","arguments":{"source_id":999999}}}' \
    '"isError":true'

# Test 12: close_port on unopened port fails gracefully
check "close_port on unopened port fails gracefully" \
    '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"close_port","arguments":{"source_id":999999}}}' \
    '"isError":true'

# Test 13: read_buffer on unopened port returns empty
check "read_buffer on unopened port returns empty" \
    '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"read_buffer","arguments":{"source_id":999999}}}' \
    '"text":"\[\]"'

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
