# Forward all requested goals in one invocation, preserving parallel builds.
.DEFAULT_GOAL := all
forward_targets := all static-libs shared-libs tools pkgconf install clean help \
                   test check check-caseless sanitize check-bus version \
                   deb deb_git deb_check deb_verify pkg pkg_git pkg_check pkg_verify \
                   clean_packages check-packaging scan-build tidy
.PHONY: $(forward_targets) forward
$(forward_targets): forward

forward:
	$(MAKE) -C src $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
