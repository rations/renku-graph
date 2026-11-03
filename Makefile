# SPDX-License-Identifier: MIT
# Builds on RenkuOS only (needs the Media Kit, Midi Kit 2 and rc).
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -MMD -MP
LDFLAGS = -lbe -lmedia -lmidi2

SOURCES = src/App.cpp src/MainWindow.cpp src/GraphView.cpp \
          src/GraphModel.cpp src/MediaWorker.cpp src/MidiWorker.cpp \
          src/Settings.cpp src/SettingsWindow.cpp src/ParameterWindow.cpp \
          src/RenameWindow.cpp
OBJECTS = $(patsubst src/%.cpp,objects/%.o,$(SOURCES))
DEPS    = $(OBJECTS:.o=.d)
TARGET  = RenkuGraph
RDEF    = RenkuGraph.rdef
RSRC    = objects/RenkuGraph.rsrc

$(TARGET): $(OBJECTS) $(RSRC)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	xres -o $(TARGET) $(RSRC)
	mimeset -f $(TARGET)

objects/%.o: src/%.cpp | objects
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RSRC): $(RDEF) | objects
	rc -o $@ $<

objects:
	mkdir -p objects

-include $(DEPS)

clean:
	rm -rf objects $(TARGET)

.PHONY: clean
