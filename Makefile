.PHONY: build clean rebuild test

BUILDDIR = build
TARGET = apple-midi-mcp

build:
	@mkdir -p $(BUILDDIR)
	@cd $(BUILDDIR) && cmake .. && make -j$(shell sysctl -n hw.ncpu)
	@ln -sf $(BUILDDIR)/$(TARGET) $(TARGET)

clean:
	@rm -rf $(BUILDDIR) $(TARGET)

rebuild: clean build

test: build
	@bash test/test_protocol.sh ./$(TARGET)
