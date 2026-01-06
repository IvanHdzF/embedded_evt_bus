# Makefile — evt_bus

BUILD_DIR   ?= build
BUILD_TYPE  ?= Debug
GENERATOR   ?= MinGW Makefiles
CMAKE_ARGS  ?=

# FreeRTOS port
FREERTOS_INC ?=
FREERTOS_CFG ?=

.PHONY: all configure build test clean rebuild port_freertos_stub port_freertos_real

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)

build: configure
	cmake --build $(BUILD_DIR)

test:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DEVT_BUS_BUILD_TESTS=ON \
		-DEVT_BUS_ENABLE_FREERTOS=OFF \
		$(CMAKE_ARGS)
	cmake --build $(BUILD_DIR)
	cd $(BUILD_DIR) && ctest --output-on-failure

# Compile-check the FreeRTOS port using stub headers (no real FreeRTOS needed)
port_freertos_stub:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DEVT_BUS_BUILD_TESTS=OFF \
		-DEVT_BUS_ENABLE_FREERTOS=ON \
		-DEVT_BUS_FREERTOS_STUB=ON \
		$(CMAKE_ARGS)
	cmake --build $(BUILD_DIR)

# Build FreeRTOS port with real FreeRTOS includes provided via CMAKE_ARGS
# Example:
#   make port_freertos_real FREERTOS_INC="...;..."'
port_freertos_real:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DEVT_BUS_BUILD_TESTS=OFF \
		-DEVT_BUS_ENABLE_FREERTOS=ON \
		-DEVT_BUS_FREERTOS_STUB=OFF \
		-DFREERTOS_INCLUDE_DIRS="$(FREERTOS_INC);$(FREERTOS_CFG) "\
		$(CMAKE_ARGS)
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean build
