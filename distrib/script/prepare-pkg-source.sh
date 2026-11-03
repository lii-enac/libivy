#!/usr/bin/env bash
# Download/copy one release archive and pin the exact bytes used by makepkg.
set -euo pipefail
recipe_path=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
recipe_dir=$(dirname "$recipe_path")
# This is the locally generated, trusted PKGBUILD, also sourced by makepkg.
source "$recipe_path"
if [ "${#source[@]}" -ne 1 ]; then
    echo 'Expected exactly one release source archive' >&2
    exit 1
fi
archive_source=${source[0]}
archive_source=${archive_source#*::}
archive_temp=$(mktemp "$recipe_dir/.release-source.XXXXXX")
recipe_temp=$(mktemp "$recipe_dir/.PKGBUILD.XXXXXX")
trap 'rm -f "$archive_temp" "$recipe_temp"' EXIT
case "$archive_source" in
    https://*|http://*)
        curl --fail --location --retry 2 --show-error --output "$archive_temp" "$archive_source"
        ;;
    file://*) cp "${archive_source#file://}" "$archive_temp" ;;
    /*) cp "$archive_source" "$archive_temp" ;;
    *) cp "$recipe_dir/$archive_source" "$archive_temp" ;;
esac
if [ ! -s "$archive_temp" ]; then
    echo 'Downloaded archive is empty' >&2
    exit 1
fi
if command -v sha256sum >/dev/null; then
    archive_sha256=$(sha256sum "$archive_temp" | awk '{print $1}')
else
    archive_sha256=$(shasum -a 256 "$archive_temp" | awk '{print $1}')
fi
if [[ ! $archive_sha256 =~ ^[0-9a-f]{64}$ ]]; then
    echo 'Could not calculate SHA-256' >&2
    exit 1
fi
# A local source plus SRCDEST=$recipe_dir ensures makepkg uses these bytes.
sed -e "s|^source=.*|source=('release-source.tar.gz')|" \
    -e "s|^sha256sums=.*|sha256sums=('$archive_sha256')|" \
    "$recipe_path" > "$recipe_temp"
mv -f "$archive_temp" "$recipe_dir/release-source.tar.gz"
printf '%s\n' "$archive_source" > "$recipe_dir/release-source.url"
mv -f "$recipe_temp" "$recipe_path"
printf 'Source: %s\nSHA-256: %s\n' "$archive_source" "$archive_sha256"
