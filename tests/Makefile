CC ?= cc
PKG_CONFIG ?= pkg-config
PCRE_PKG ?= libpcre2-8
IVYBIND ?= ../src/ivybind.c
PCRE_OPT ?= 0
CFLAGS ?= -O1 -g -Wall -Wextra
BUILD_DIR ?= build
REGEX_FLAGS = -DUSE_PCRE_REGEX -DPCRE_OPT=$(PCRE_OPT)

.PHONY: check sanitize clean
check: $(BUILD_DIR)/regex
	./$(BUILD_DIR)/regex

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
