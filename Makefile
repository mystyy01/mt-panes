CXX ?= c++

SRC_DIR := src
BUILD_DIR := build
TARGET := mt-panes

CXXSTD := -std=c++20
WARN := -Wall -Wextra -Wpedantic
DEPFLAGS := -MMD -MP

CXXFLAGS ?= $(CXXSTD) $(WARN)
LDFLAGS ?=

SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC))
DEPS := $(OBJ:.o=.d)

.PHONY: all release debug run clean

all: release

release: CXXFLAGS += -O2 -DNDEBUG
release: $(TARGET)

debug: CXXFLAGS += -O0 -g -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
