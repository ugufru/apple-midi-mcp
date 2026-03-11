#include "midi_bridge.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <unistd.h>

MidiBridge::MidiBridge() {
    OSStatus status = MIDIClientCreate(
        CFSTR("AppleMidiMCP"),
        nullptr, // notify proc
        nullptr, // notify refCon
        &client_
    );
    if (status != noErr) {
        std::cerr << "[midi] Failed to create MIDI client: " << status << std::endl;
        return;
    }

    status = MIDIOutputPortCreate(client_, CFSTR("Output"), &outputPort_);
    if (status != noErr) {
        std::cerr << "[midi] Failed to create output port: " << status << std::endl;
    }

    status = MIDIInputPortCreate(client_, CFSTR("Input"), readProc, this, &inputPort_);
    if (status != noErr) {
        std::cerr << "[midi] Failed to create input port: " << status << std::endl;
    }
}

MidiBridge::~MidiBridge() {
    // Disconnect all open sources
    for (auto& [id, endpoint] : openSources_) {
        MIDIPortDisconnectSource(inputPort_, endpoint);
    }
    if (inputPort_) MIDIPortDispose(inputPort_);
    if (outputPort_) MIDIPortDispose(outputPort_);
    if (client_) MIDIClientDispose(client_);
}

std::vector<MIDIDeviceInfo> MidiBridge::listDevices() {
    std::vector<MIDIDeviceInfo> devices;

    // Enumerate sources (inputs)
    ItemCount sourceCount = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < sourceCount; i++) {
        MIDIEndpointRef endpoint = MIDIGetSource(i);
        MIDIDeviceInfo info;
        info.name = getEndpointName(endpoint);
        info.uniqueId = getEndpointUniqueId(endpoint);
        info.isSource = true;
        devices.push_back(info);
    }

    // Enumerate destinations (outputs)
    ItemCount destCount = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < destCount; i++) {
        MIDIEndpointRef endpoint = MIDIGetDestination(i);
        MIDIDeviceInfo info;
        info.name = getEndpointName(endpoint);
        info.uniqueId = getEndpointUniqueId(endpoint);
        info.isSource = false;
        devices.push_back(info);
    }

    return devices;
}

bool MidiBridge::sendMessage(int destinationId, const std::vector<uint8_t>& bytes) {
    MIDIEndpointRef dest = findDestination(destinationId);
    if (!dest) {
        std::cerr << "[midi] Destination not found: " << destinationId << std::endl;
        return false;
    }

    // Log SysEx sends
    if (!bytes.empty() && bytes[0] == 0xF0) {
        std::cerr << "[midi] Sending SysEx (" << bytes.size() << " bytes) via MIDISend to dest " << destinationId << std::endl;
    }

    // Regular MIDI messages use MIDISend
    uint8_t buffer[1024];
    MIDIPacketList* packetList = reinterpret_cast<MIDIPacketList*>(buffer);
    MIDIPacket* packet = MIDIPacketListInit(packetList);
    packet = MIDIPacketListAdd(packetList, sizeof(buffer), packet, 0, bytes.size(), bytes.data());

    if (!packet) {
        std::cerr << "[midi] Failed to build MIDI packet" << std::endl;
        return false;
    }

    OSStatus status = MIDISend(outputPort_, dest, packetList);
    if (status != noErr) {
        std::cerr << "[midi] Failed to send MIDI message: " << status << std::endl;
        return false;
    }

    return true;
}

bool MidiBridge::openPort(int sourceId) {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    if (openSources_.count(sourceId)) {
        return true; // Already open
    }

    MIDIEndpointRef source = findSource(sourceId);
    if (!source) {
        std::cerr << "[midi] Source not found: " << sourceId << std::endl;
        return false;
    }

    // Store sourceId as the connRefCon so we know which source sent data
    OSStatus status = MIDIPortConnectSource(inputPort_, source, reinterpret_cast<void*>(static_cast<intptr_t>(sourceId)));
    if (status != noErr) {
        std::cerr << "[midi] Failed to connect source: " << status << std::endl;
        return false;
    }

    openSources_[sourceId] = source;
    buffers_[sourceId] = {};
    return true;
}

std::vector<MIDIMessageData> MidiBridge::readBuffer(int sourceId) {
    std::lock_guard<std::mutex> lock(bufferMutex_);
    std::vector<MIDIMessageData> result;

    auto it = buffers_.find(sourceId);
    if (it != buffers_.end()) {
        result.assign(it->second.begin(), it->second.end());
        it->second.clear(); // FIFO: flush after read
    }

    return result;
}

bool MidiBridge::closePort(int sourceId) {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    auto it = openSources_.find(sourceId);
    if (it == openSources_.end()) {
        return false;
    }

    MIDIPortDisconnectSource(inputPort_, it->second);
    openSources_.erase(it);
    buffers_.erase(sourceId);
    return true;
}

// Static callback — routes to instance method
void MidiBridge::readProc(const MIDIPacketList* pktlist, void* readProcRefCon, void* srcConnRefCon) {
    auto* self = static_cast<MidiBridge*>(readProcRefCon);
    int sourceId = static_cast<int>(reinterpret_cast<intptr_t>(srcConnRefCon));
    self->handlePackets(pktlist, sourceId);
}

void MidiBridge::handlePackets(const MIDIPacketList* pktlist, int sourceId) {
    std::lock_guard<std::mutex> lock(bufferMutex_);

    auto it = buffers_.find(sourceId);
    if (it == buffers_.end()) return;

    dbgCallbacks_++;
    const MIDIPacket* packet = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; i++) {
        dbgPackets_++;
        const uint8_t* data = packet->data;
        UInt16 len = packet->length;

        // Debug: log every incoming packet
        std::cerr << "[midi-rx] src=" << sourceId << " pkt " << i
                  << "/" << pktlist->numPackets << " len=" << len;
        if (len > 0) {
            std::cerr << " first=0x" << std::hex << (int)data[0];
            if (len > 1) std::cerr << " last=0x" << (int)data[len-1];
            std::cerr << std::dec;
        }
        std::cerr << std::endl;

        if (len > 0 && data[0] == 0xF0) {
            dbgSysexStarts_++;
            // Start of SysEx — check if complete in this packet
            auto& pending = sysexPending_[sourceId];
            pending.assign(data, data + len);
            if (data[len - 1] == 0xF7) {
                // Complete SysEx in one packet
                dbgSysexComplete_++;
                MIDIMessageData msg;
                msg.bytes = std::move(pending);
                msg.timestamp = packet->timeStamp;
                it->second.push_back(std::move(msg));
                pending.clear();
            }
        } else if (!sysexPending_[sourceId].empty()) {
            // Continuation of SysEx fragment
            auto& pending = sysexPending_[sourceId];
            pending.insert(pending.end(), data, data + len);
            if (len > 0 && data[len - 1] == 0xF7) {
                // SysEx complete
                dbgSysexComplete_++;
                MIDIMessageData msg;
                msg.bytes = std::move(pending);
                msg.timestamp = packet->timeStamp;
                it->second.push_back(std::move(msg));
                pending.clear();
            }
        } else {
            // Regular MIDI message
            dbgRegularMsgs_++;
            MIDIMessageData msg;
            msg.bytes.assign(data, data + len);
            msg.timestamp = packet->timeStamp;
            it->second.push_back(std::move(msg));
        }

        packet = MIDIPacketNext(packet);
    }
}

MIDIEndpointRef MidiBridge::findDestination(int uniqueId) {
    ItemCount count = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = MIDIGetDestination(i);
        if (getEndpointUniqueId(endpoint) == uniqueId) return endpoint;
    }
    return 0;
}

MIDIEndpointRef MidiBridge::findSource(int uniqueId) {
    ItemCount count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = MIDIGetSource(i);
        if (getEndpointUniqueId(endpoint) == uniqueId) return endpoint;
    }
    return 0;
}

std::string MidiBridge::getEndpointName(MIDIEndpointRef endpoint) {
    CFStringRef name = nullptr;
    MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
    if (!name) {
        MIDIObjectGetStringProperty(endpoint, kMIDIPropertyName, &name);
    }
    if (!name) return "(unknown)";

    char buf[256];
    CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8);
    CFRelease(name);
    return std::string(buf);
}

int MidiBridge::getEndpointUniqueId(MIDIEndpointRef endpoint) {
    SInt32 uid = 0;
    MIDIObjectGetIntegerProperty(endpoint, kMIDIPropertyUniqueID, &uid);
    return static_cast<int>(uid);
}
