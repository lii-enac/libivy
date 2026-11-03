# Included by src/Makefile.targets; package outputs live at the repository root.
package_root := $(abspath ..)
package_host := $(shell uname -s)
package_machine := $(shell uname -m)
package_path = $(if $(filter /%,$(1)),$(1),$(package_root)/$(1))
package_build_root ?= $(package_root)/build
override package_build_root := $(call package_path,$(package_build_root))

deb_version ?= $(VERSION)
deb_arch ?= $(shell dpkg --print-architecture 2>/dev/null)
deb_make_jobs ?= 6
deb_date ?= $(shell LC_ALL=C date '+%a, %d %b %Y %H:%M:%S %z')
deb_build_dir ?= $(package_build_root)/deb-build/$(package_host)-$(package_machine)
deb_package ?= $(package_build_root)/deb/libivy_$(deb_version)_$(deb_arch).deb
deb_git_version ?= $(VERSION).git$(shell date +%Y%m%d).$(shell git rev-parse --short HEAD 2>/dev/null || echo local)
deb_git_build_dir ?= $(package_build_root)/deb-build-git/$(package_host)-$(package_machine)
deb_git_package ?= $(package_build_root)/deb/libivy_$(deb_git_version)_$(deb_arch).deb

# Keep package filenames lazy: their Debian defaults run dpkg/git/date.
# Normalize those paths only in the packaging recipes, never during a build.
override deb_build_dir := $(call package_path,$(deb_build_dir))
override deb_git_build_dir := $(call package_path,$(deb_git_build_dir))

.PHONY: deb deb_git deb_check deb_verify pkg pkg_git pkg_check pkg_verify clean_packages
deb_check:
	@test "$(package_host)" = Linux || { echo "make deb/deb_git requires Linux (current OS: $(package_host))"; exit 1; }
	@for tool in dpkg dpkg-deb dpkg-shlibdeps fakeroot strip tar gzip; do command -v $$tool >/dev/null || { echo "missing dependency: $$tool"; exit 1; }; done
	@test -n "$(deb_arch)" || { echo "cannot determine Debian architecture"; exit 1; }
	@pkg-config --exists libpcre2-8 || { echo "missing dependency: libpcre2-dev"; exit 1; }

deb: deb_check
	sh "$(package_root)/tools/package-deb.sh" "$(package_root)" "$(deb_build_dir)" "$(call package_path,$(deb_package))" "$(deb_version)" "$(deb_arch)" "$(deb_make_jobs)" "$(deb_date)"

deb_git: deb_check
	sh "$(package_root)/tools/package-deb.sh" "$(package_root)" "$(deb_git_build_dir)" "$(call package_path,$(deb_git_package))" "$(deb_git_version)" "$(deb_arch)" "$(deb_make_jobs)" "$(deb_date)"

deb_verify:
	dpkg-deb --info "$(call package_path,$(deb_package))"
	dpkg-deb --contents "$(call package_path,$(deb_package))"
	@if command -v lintian >/dev/null; then lintian "$(call package_path,$(deb_package))"; else echo "lintian not found, skipping lintian check"; fi

pkg_version ?= $(VERSION)
pkg_make_jobs ?= 6
pkg_prefix ?= /mingw64
pkg_source ?= https://github.com/lii-enac/libivy/archive/refs/tags/$(VERSION).tar.gz
pkg_source_dir ?= libivy-$(VERSION)
pkg_source_sha256 ?=
pkg_build_dir ?= $(package_build_root)/pkg-build/$(package_host)-$(package_machine)
pkg_package_glob ?= $(package_build_root)/libivy-$(pkg_version)-*.pkg.tar.*
pkg_git_version ?= $(VERSION).git
pkg_git_branch ?= master
pkg_git_source ?= git+https://github.com/lii-enac/libivy.git\#branch=$(pkg_git_branch)
pkg_git_source_dir ?= libivy
pkg_git_build_dir ?= $(package_build_root)/pkg-build-git/$(package_host)-$(package_machine)
pkg_git_package_glob ?= $(package_build_root)/libivy-$(pkg_git_version)-*.pkg.tar.*

override pkg_build_dir := $(call package_path,$(pkg_build_dir))
override pkg_git_build_dir := $(call package_path,$(pkg_git_build_dir))
override pkg_package_glob := $(call package_path,$(pkg_package_glob))
override pkg_git_package_glob := $(call package_path,$(pkg_git_package_glob))

pkg_check:
	@case "$(package_host)" in MINGW*|MSYS*) ;; *) echo "make pkg/pkg_git requires MSYS2/MinGW64 (current OS: $(package_host))"; exit 1;; esac
	@command -v makepkg >/dev/null || { echo "missing dependency: makepkg"; exit 1; }

pkg: pkg_check
	mkdir -p "$(package_build_root)" "$(pkg_build_dir)"
	sed -e "s|@PKG_VERSION@|$(pkg_version)|g; s|@PKG_PREFIX@|$(pkg_prefix)|g; s|@PKG_MAKE_JOBS@|$(pkg_make_jobs)|g; s|@PKG_SOURCE@|$(pkg_source)|g; s|@PKG_SOURCE_DIR@|$(pkg_source_dir)|g; s|@PKG_SOURCE_SHA256@|$(pkg_source_sha256)|g; s|@VCS_DEPENDENCY@||g" "$(package_root)/distrib/PKGBUILD.proto" > "$(pkg_build_dir)/PKGBUILD"
	@test -n "$(pkg_source_sha256)" || { echo "missing pkg_source_sha256; PKGBUILD generated: $(pkg_build_dir)/PKGBUILD"; echo "Generate the hash with: cd $(pkg_build_dir) && makepkg -g"; exit 1; }
	cd "$(pkg_build_dir)" && PKGDEST="$(package_build_root)" makepkg -f --cleanbuild
	@ls -lh $(pkg_package_glob)

pkg_git: pkg_check
	mkdir -p "$(package_build_root)" "$(pkg_git_build_dir)"
	sed -e "s|@PKG_VERSION@|$(pkg_git_version)|g; s|@PKG_PREFIX@|$(pkg_prefix)|g; s|@PKG_MAKE_JOBS@|$(pkg_make_jobs)|g; s|@PKG_SOURCE@|$(pkg_git_source)|g; s|@PKG_SOURCE_DIR@|$(pkg_git_source_dir)|g; s|@PKG_SOURCE_SHA256@|SKIP|g; s|@VCS_DEPENDENCY@|'git'|g" "$(package_root)/distrib/PKGBUILD.proto" > "$(pkg_git_build_dir)/PKGBUILD"
	cd "$(pkg_git_build_dir)" && PKGDEST="$(package_build_root)" makepkg -f --cleanbuild
	@ls -lh $(pkg_git_package_glob)

pkg_verify:
	@ls -lh $(pkg_package_glob)
	tar -tf $(pkg_package_glob)

clean_packages:
	rm -rf "$(package_build_root)/deb" "$(package_build_root)/deb-build" "$(package_build_root)/deb-build-git" "$(package_build_root)/pkg-build" "$(package_build_root)/pkg-build-git"
	rm -f "$(package_build_root)"/libivy-*.pkg.tar.*
