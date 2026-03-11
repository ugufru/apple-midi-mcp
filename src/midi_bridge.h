#pragma once

#include <CoreMIDI/CoreMIDI.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <deque>
#include <cstdint>

struct MIDIDeviceInfo {
    std::string name;
    int uniqueId;
    bool isSource; // true = input (source), false = output (destination)
};

struct MIDIMessageData {
    std::vector<uint8_t> bytes;
    uint64_t timestamp;
};

class MidiBridge {
public:
    MidiBridge();
    ~MidiBridge();

    // Enumerate all MIDI sources and destinations
    std::vector<MIDIDeviceInfo> listDevices();

    // Send a MIDI message to a destination by unique ID
    bool sendMessage(int destinationId, const std::vector<uint8_t>& bytes);

    // Open a source port for listening (buffers incoming messages)
    bool openPort(int sourceId);

    // Read and flush buffered messages from an open port
    std::vector<MIDIMessageData> readBuffer(int sourceId);

    // Close a listening port
    bool closePort(int sourceId);

private:
    MIDIClientRef client_ = 0;
    MIDIPortRef outputPort_ = 0;
    MIDIPortRef inputPort_ = 0;

    // Buffers for incoming MIDI data, keyed by source unique ID
    std::mutex bufferMutex_;
    std::map<int, std::deque<MIDIMessageData>> buffers_;
    std::map<int, MIDIEndpointRef> openSources_;
    std::map<int, std::vector<uint8_t>> sysexPending_; // SysEx reassembly per source

    // Debug counters
    std::atomic<int> dbgCallbacks_{0};
    std::atomic<int> dbgPackets_{0};
    std::atomic<int> dbgSysexStarts_{0};
    std::atomic<int> dbgSysexComplete_{0};
    std::atomic<int> dbgRegularMsgs_{0};

public:
    struct DebugStats {
        int callbacks, packets, sysexStarts, sysexComplete, regularMsgs;
    };
    DebugStats getDebugStats() const {
        return {dbgCallbacks_.load(), dbgPackets_.load(), dbgSysexStarts_.load(),
                dbgSysexComplete_.load(), dbgRegularMsgs_.load()};
    }

private:
    static void readProc(const MIDIPacketList* pktlist, void* readProcRefCon, void* srcConnRefCon);
    void handlePackets(const MIDIPacketList* pktlist, int sourceId);

    MIDIEndpointRef findDestination(int uniqueId);
    MIDIEndpointRef findSource(int uniqueId);
    static std::string getEndpointName(MIDIEndpointRef endpoint);
    static int getEndpointUniqueId(MIDIEndpointRef endpoint);
};
