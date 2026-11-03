#!/bin/sh
# Build a binary Debian package from the current working tree in a fresh copy.
set -eu
root=$1
build_parent=$2
output=$3
version=$4
arch=$5
jobs=$6
package_date=$7
mkdir -p "$build_parent" "$(dirname "$output")"
work=$(mktemp -d "$build_parent/libivy.XXXXXXXX")
trap 'rm -rf "$work"' 0
trap 'exit 1' HUP INT TERM
source_dir=$work/source
stage=$work/stage
mkdir -p "$source_dir" "$stage"
# Exclude this build even when a custom build directory is inside the source tree.
relative_work=${work#"$root"/}
tar -C "$root" --exclude="./$relative_work" --exclude='./.git' --exclude='./.vscode' \
    --exclude='./build' --exclude='./src/build' --exclude='./tests/build*' \
    --exclude='*.o' --exclude='*.a' --exclude='*.so*' --exclude='*.dylib*' \
    --exclude='*.dll*' --exclude='*.exe' --exclude='*.pc' \
    --exclude='./tools/ivyprobe' --exclude='./tools/ivyperf' \
    --exclude='./tools/ivythroughput' -cf "$work/source.tar" .
tar -xf "$work/source.tar" -C "$source_dir"
make -C "$source_dir" -j"$jobs" static-libs shared-libs tools pkgconf PREFIX=/usr LIB=/lib
make -C "$source_dir" check
make -C "$source_dir" -j"$jobs" install PREFIX=/usr LIB=/lib DESTDIR="$stage"
mkdir -p "$stage/DEBIAN" "$stage/usr/share/doc/libivy"
cp "$root/distrib/deb/triggers" "$stage/DEBIAN/triggers"
major=$(awk '$1 == "#define" && $2 == "IVYMAJOR_VERSION" { print $3 }' "$source_dir/src/version.h")
minor=$(awk '$1 == "#define" && $2 == "IVYMINOR_VERSION" { print $3 }' "$source_dir/src/version.h")
printf 'libivy %s libivy (>= %s)\n' "$major" "$version" > "$stage/DEBIAN/shlibs"
# Generate real runtime dependencies; never silently ignore unresolved libraries.
shlibs=$(cd "$source_dir" && dpkg-shlibdeps -O -xlibivy \
    -L"$stage/DEBIAN/shlibs" -S"$stage" -l"$stage/usr/lib" \
    -e"$stage/usr/lib/libivy.so.$major.$minor" \
    -e"$stage/usr/bin/ivyprobe" -e"$stage/usr/bin/ivyperf")
dependencies=$(printf '%s\n' "$shlibs" | sed -n 's/^shlibs:Depends=//p')
if [ -z "$dependencies" ]; then
    echo 'dpkg-shlibdeps did not produce runtime dependencies' >&2
    exit 1
fi
sed -e "s|@VERSION@|$version|g; s|@ARCH@|$arch|g; s|@SHLIBS_DEPENDS@|$dependencies|g" \
    "$root/distrib/deb/control" > "$stage/DEBIAN/control"
sed -e "s|@VERSION@|$version|g; s|@DATE@|$package_date|g" \
    "$root/distrib/deb/changelog" | gzip -9n > "$stage/usr/share/doc/libivy/changelog.gz"
install -m 644 "$root/distrib/deb/copyright" "$stage/usr/share/doc/libivy/copyright"
install -m 644 "$source_dir/README.md" "$stage/usr/share/doc/libivy/README.md"
strip --strip-unneeded "$stage/usr/lib/libivy.so.$major.$minor" \
    "$stage/usr/bin/ivyprobe" "$stage/usr/bin/ivyperf"
strip --strip-debug "$stage/usr/lib/libivy.a"
find "$stage" -type d -exec chmod 755 {} \;
chmod 644 "$stage/DEBIAN/"* "$stage/usr/share/doc/libivy/"*
fakeroot dpkg-deb --build "$stage" "$output"
printf 'Debian package created: %s\n' "$output"
