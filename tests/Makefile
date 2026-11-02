CC ?= cc
PKG_CONFIG ?= pkg-config
PCRE_PKG ?= libpcre2-8
IVYBIND ?= ../src/ivybind.c
PCRE_OPT ?= 0
CFLAGS ?= -O1 -g -Wall -Wextra
BUILD_DIR ?= build
REGEX_FLAGS = -DUSE_PCRE_REGEX -DPCRE_OPT=$(PCRE_OPT)

.PHONY: check sanitize clean
check: $(BUILD_DIR)/regex $(BUILD_DIR)/discovery $(BUILD_DIR)/receive $(BUILD_DIR)/fifo
	./$(BUILD_DIR)/regex
	./$(BUILD_DIR)/discovery
	./$(BUILD_DIR)/receive
	./$(BUILD_DIR)/fifo

$(BUILD_DIR)/regex: regex.c $(IVYBIND) ../src/ivybind.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(REGEX_FLAGS) -I../src \
		$(shell $(PKG_CONFIG) --cflags $(PCRE_PKG)) regex.c $(IVYBIND) \
		-o $@ $(LDFLAGS) $(shell $(PKG_CONFIG) --libs $(PCRE_PKG))

sanitize:
	$(MAKE) check BUILD_DIR=build-sanitize \
		CFLAGS='-O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer' \
		LDFLAGS='-fsanitize=address,undefined'

clean:
	rm -rf build build-*

# Build the library first (src/Makefile on Linux, src/Makefile.osx on macOS).
IVY_LIB ?= ../src/libivy.a
.PHONY: check-bus
check-bus: $(BUILD_DIR)/bus
	python3 bus.py ./$(BUILD_DIR)/bus

$(BUILD_DIR)/bus: bus.c $(IVY_LIB)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I../src bus.c $(IVY_LIB) -o $@ \
		$(LDFLAGS) $(shell $(PKG_CONFIG) --libs $(PCRE_PKG))

.PHONY: check-caseless
check-caseless:
	$(MAKE) check BUILD_DIR=build-caseless PCRE_OPT=PCRE2_CASELESS CPPFLAGS=-DTEST_CASELESS

$(BUILD_DIR)/discovery: discovery.c ../src/ivydiscovery.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I../src discovery.c -o $@ $(LDFLAGS)

# receive.c includes ivy.c so its private protocol receiver is instrumented too.
RECEIVE_SOURCES = $(addprefix ../src/,ivyloop.c timer.c ivysocket.c ivybuffer.c ivyfifo.c ivybind.c intervalRegexp.c param.c)
ifeq ($(OS),Windows_NT)
RECEIVE_CPPFLAGS = -DWIN32
RECEIVE_LIBS = -lws2_32
endif
$(BUILD_DIR)/receive: receive.c ../src/ivy.c ../src/ivydiscovery.h $(RECEIVE_SOURCES) $(wildcard ../src/*.h)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(RECEIVE_CPPFLAGS) $(CFLAGS) $(REGEX_FLAGS) -I../src \
		$(shell $(PKG_CONFIG) --cflags $(PCRE_PKG)) receive.c $(RECEIVE_SOURCES) \
		-o $@ $(LDFLAGS) $(shell $(PKG_CONFIG) --libs $(PCRE_PKG)) $(RECEIVE_LIBS)

FIFO_SUPPORT = ../src/timer.c ../src/ivybuffer.c ../src/param.c
$(BUILD_DIR)/fifo: fifo.c ../src/ivyfifo.c ../src/ivysocket.c ../src/ivyloop.c $(FIFO_SUPPORT) $(wildcard ../src/*.h)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(RECEIVE_CPPFLAGS) $(CFLAGS) -I../src fifo.c $(FIFO_SUPPORT) \
		-o $@ $(LDFLAGS) $(RECEIVE_LIBS)
