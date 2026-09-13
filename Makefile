# ============================================================
# VCV Rack 2.6.x Plugin Makefile — Arch Linux
# Usage:
#   export RACK_DIR=/path/to/Rack-SDK-2.6.6
#   make
# ============================================================

# Slug must match plugin.json
SLUG = SysExBank

# Source files
SOURCES = \
	src/plugin.cpp \
	src/module.cpp \
	src/ui.cpp

# Extra include dirs (none needed beyond Rack SDK)
CXXFLAGS_EXTRA =

# ============================================================
# Rack SDK boilerplate — include Rack's build system
# ============================================================
ifndef RACK_DIR
  $(error RACK_DIR is not set. Export it: export RACK_DIR=/path/to/Rack-SDK)
endif

include $(RACK_DIR)/arch.mk

FLAGS  += -std=c++17 -fPIC
FLAGS  += -I$(RACK_DIR)/include -I$(RACK_DIR)/dep/include
FLAGS  += $(CXXFLAGS_EXTRA)

LDFLAGS += -shared -fPIC
LDFLAGS += -L$(RACK_DIR) -lRack

# Output
TARGET = plugin.so

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Build rules
.PHONY: all clean dist

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(FLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

dist: all
	@mkdir -p dist/$(SLUG)
	cp $(TARGET) dist/$(SLUG)/
	cp plugin.json dist/$(SLUG)/
	@if [ -d res ]; then cp -r res dist/$(SLUG)/; fi
	@echo "Distribution ready in dist/$(SLUG)/"

# Dependency tracking
-include $(OBJECTS:.o=.d)

%.o: %.cpp
	$(CXX) $(FLAGS) -MMD -MP -c -o $@ $<
