#!/bin/sh
# Install dependencies for the native libivy backend; skip installed packages.
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

is_installed() {
    case "$manager" in
        brew) brew list --formula --versions "$1" >/dev/null 2>&1 ;;
        apt) [ "$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null)" = 'install ok installed' ] ;;
        pacman) pacman -Q "$1" >/dev/null 2>&1 ;;
    esac
}

set --
# Package names above are fixed identifiers, intentionally split into arguments.
for dependency in $dependencies; do
    if [ "$action" = upgrade ] || ! is_installed "$dependency"; then
        set -- "$@" "$dependency"
    fi
done
if [ "$#" -eq 0 ]; then
    echo 'All libivy build dependencies are already installed.'
    exit 0
fi

case "$manager:$action" in
    brew:install) brew install "$@" ;;
    brew:upgrade) brew upgrade "$@" ;;
    apt:install) as_root apt install -y --no-reinstall "$@" ;;
    apt:upgrade) as_root apt install -y --only-upgrade "$@" ;;
    pacman:install) pacman -S --needed "$@" ;;
    pacman:upgrade) pacman -Syu --needed "$@" ;;
esac
