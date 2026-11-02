# Forward all requested goals in one invocation, preserving parallel builds.
.DEFAULT_GOAL := all
.PHONY: all static-libs shared-libs tools pkgconf install clean help test check check-caseless sanitize check-bus version forward
all static-libs shared-libs tools pkgconf install clean help test check check-caseless sanitize check-bus version: forward

forward:
	$(MAKE) -C src $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
