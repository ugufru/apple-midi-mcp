CXX = clang++
SYSROOT := $(shell xcrun --show-sdk-path)
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I deps -isysroot $(SYSROOT) -I$(SYSROOT)/usr/include/c++/v1
LDFLAGS = -framework CoreMIDI -framework CoreFoundation
TARGET = apple-midi-mcp
SRCDIR = src
BUILDDIR = build

SRCS = $(SRCDIR)/main.cpp $(SRCDIR)/mcp_handler.cpp $(SRCDIR)/midi_bridge.cpp
OBJS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

.PHONY: all clean
