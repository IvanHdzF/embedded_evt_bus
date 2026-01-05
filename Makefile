# Makefile — evt_bus

BUILD_DIR   ?= build
BUILD_TYPE  ?= Debug
GENERATOR   ?= MinGW Makefiles

.PHONY: all configure build test clean rebuild

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR)

test: configure
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DEVT_BUS_BUILD_TESTS=ON
	cmake --build $(BUILD_DIR)
	cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build
