#!/bin/sh
# Install/update native libivy dependencies without reinstalling current versions.
set -eu
action=${1:-install}
case "$action" in install|upgrade) ;; *) echo "Unknown action: $action" >&2; exit 1;; esac
case "$(uname -s)" in
    Darwin)
        manager=brew
        dependencies='pcre2 pkgconf'
        ;;
    Linux)
        manager=apt
        dependencies='build-essential pkgconf libpcre2-dev'
        ;;
    MINGW*|MSYS*)
        manager=pacman
        dependencies='make mingw-w64-x86_64-gcc mingw-w64-x86_64-pcre2 mingw-w64-x86_64-pkgconf'
        ;;
    *) echo 'Supported systems: macOS (Homebrew), Linux (APT), MSYS2 MinGW64 (pacman)' >&2; exit 1 ;;
esac
command -v "$manager" >/dev/null 2>&1 || { echo "Missing package manager: $manager" >&2; exit 1; }

as_root() {
    if [ "$(id -u)" = 0 ]; then "$@"; else sudo "$@"; fi
}

# Let the manager install missing packages and upgrade outdated ones.
# Current versions are skipped by install / --no-reinstall / --needed.
set -- $dependencies

case "$manager:$action" in
    brew:install) brew update; HOMEBREW_NO_ASK=1 brew install "$@" ;;
    brew:upgrade) HOMEBREW_NO_ASK=1 brew upgrade "$@" ;;
    apt:install) export DEBIAN_FRONTEND=noninteractive; as_root apt update; as_root apt install -y --no-reinstall "$@" ;;
    apt:upgrade) as_root apt install -y --only-upgrade "$@" ;;
    pacman:install) pacman -S --needed --noconfirm "$@" ;;
    pacman:upgrade) pacman -Syu --needed --noconfirm "$@" ;;
esac
