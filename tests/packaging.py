#!/usr/bin/env python3
"""Portable packaging recipe tests with fixture compilers/package tools.

These check orchestration, checksums and staged paths; they do not replace
native dpkg-deb/makepkg builds on Linux and MSYS2.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MAKE = shutil.which("make")


def run(args, cwd=ROOT, env=None, success=True):
    result = subprocess.run(args, cwd=cwd, env=env, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != success:
        raise AssertionError(result.stdout)
    return result.stdout


def executable(path, content):
    path.write_text(content)
    path.chmod(0o755)


def snapshot(destination):
    shutil.copytree(ROOT, destination, ignore=shutil.ignore_patterns(
        ".git", ".vscode", "build", "build-*", "*.o", "*.a", "*.so*",
        "*.dylib*", "*.dll*", "*.exe", "*.pc", "ivyprobe", "ivyperf",
        "ivythroughput"))


with tempfile.TemporaryDirectory(prefix="libivy-packaging-") as name:
    temporary = Path(name)
    source = temporary / "source"
    snapshot(source)
    tools = temporary / "bin"
    tools.mkdir()
    env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"])
    # Fake makepkg only records the generated recipe and creates a fixture archive.
    executable(tools / "makepkg", '''#!/bin/sh
set -eu
printf '%s\\n' "$@" > "$PKGDEST/makepkg-arguments"
. ./PKGBUILD
mkdir -p "$PKGDEST/fixture"
printf fixture > "$PKGDEST/fixture/manifest"
tar -czf "$PKGDEST/$pkgname-$pkgver-1-x86_64.pkg.tar.gz" -C "$PKGDEST/fixture" manifest
''')
    common = [MAKE, "-s", "package_host=MINGW64_NT", "package_build_root=" + str(temporary / "packages")]
    output = run(common + ["pkg"], cwd=source, env=env, success=False)
    assert "missing pkg_source_sha256" in output
    assert not (temporary / "packages/makepkg-arguments").exists()
    checksum = "a" * 64
    run(common + ["pkg", "pkg_source_sha256=" + checksum], cwd=source, env=env)
    recipes = list((temporary / "packages/pkg-build").glob("*/PKGBUILD"))
    assert len(recipes) == 1
    text = recipes[0].read_text()
    assert "sha256sums=('" + checksum + "')" in text
    assert "options=('staticlibs')" in text
    assert "mingw-w64-x86_64-pcre2" in text
    assert "SKIP" not in text and "@PKG_" not in text
    run(["bash", "-n", str(recipes[0])])
    run(common + ["pkg_git", "pkg_git_branch=packaging-test"], cwd=source, env=env)
    git_recipe = next((temporary / "packages/pkg-build-git").glob("*/PKGBUILD"))
    text = git_recipe.read_text()
    assert "#branch=packaging-test" in text
    assert "sha256sums=('SKIP')" in text and "'git'" in text
    run(["bash", "-n", str(git_recipe)])
    assert "--cleanbuild" in (temporary / "packages/makepkg-arguments").read_text()

    # Exercise actual MinGW install recipes with compiler output fixtures.
    executable(tools / "pkg-config", '''#!/bin/sh
case "$1" in --libs) echo -lpcre2-8;; --cflags) echo -I/mingw64/include;; esac
''')
    blank = temporary / "blank.c"
    blank.write_text("int packaging_fixture;\n")
    run(["cc", "-c", str(blank), "-o", str(temporary / "blank.o")])
    compiler = tools / "fixture-cc"
    executable(compiler, '''#!/usr/bin/env python3
import pathlib, shutil, sys
args = sys.argv[1:]
out = args[args.index('-o') + 1] if '-o' in args else pathlib.Path(args[-1]).with_suffix('.o')
if '-c' in args:
    shutil.copyfile(sys.argv[0] + '.o', out)
else:
    pathlib.Path(out).write_bytes(b'packaging fixture')
for arg in args:
    if arg.startswith('-Wl,--out-implib,'):
        pathlib.Path(arg.split(',', 2)[2]).write_bytes(b'import library fixture')
''')
    shutil.copyfile(temporary / "blank.o", str(compiler) + ".o")
    stage = temporary / "stage"
    run([MAKE, "-s", "-j4", "-f", "Makefile.mingw", "install",
         "CC=" + str(compiler), "PREFIX=/mingw64", "DESTDIR=" + str(stage)],
        cwd=source / "src", env=env)
    for relative in ["bin/libivy.dll", "bin/ivyprobe.exe", "bin/ivyperf.exe",
                     "lib/libivy.a", "lib/libivy.dll.a", "lib/pkgconfig/libivy.pc",
                     "include/Ivy/ivy.h", "include/Ivy/version.h"]:
        assert (stage / "mingw64" / relative).is_file(), relative
    assert not (stage / "usr").exists()
    assert not list(stage.rglob("*.so*"))
    pc = (stage / "mingw64/lib/pkgconfig/libivy.pc").read_text()
    assert "prefix=/mingw64" in pc and "-lpcre2-8" in pc and "-lws2_32" in pc
    assert str(stage) not in pc
    # A normal clean removes DLL, import/static libraries, metadata and executables.
    run([MAKE, "-s", "-f", "Makefile.mingw", "clean"], cwd=source / "src", env=env)
    assert not (source / "src/libivy.dll").exists()
    assert not (source / "tools/ivyprobe.exe").exists()

    # Validate Debian orchestration with fixture build/dependency/package tools.
    # This deliberately makes a tar fixture, not a real .deb.
    (source / "working-tree-marker").write_text("uncommitted source included")
    (source / "src/stale.o").write_text("must not enter the build copy")
    executable(tools / "make", '''#!/usr/bin/env python3
import os, pathlib, sys
args = sys.argv[1:]
root = pathlib.Path(args[args.index('-C') + 1])
assert (root / 'working-tree-marker').exists()
assert not (root / 'src/stale.o').exists()
with open(os.environ['IVY_PACKAGE_TEST_LOG'], 'a') as log:
    log.write(' '.join(args) + '\\n')
if 'install' in args:
    stage = pathlib.Path(next(arg.split('=', 1)[1] for arg in args if arg.startswith('DESTDIR=')))
    for name in ['usr/lib/libivy.so.3.16', 'usr/lib/libivy.a', 'usr/bin/ivyprobe', 'usr/bin/ivyperf', 'usr/include/Ivy/ivy.h']:
        path = stage / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('packaging fixture')
    for name in ['libivy.so', 'libivy.so.3']:
        (stage / 'usr/lib' / name).symlink_to('libivy.so.3.16')
''')
    executable(tools / "dpkg-shlibdeps", '''#!/bin/sh
set -eu
if [ "${IVY_PACKAGE_TEST_MISSING_DEPS:-0}" = 1 ]; then exit 1; fi
for arg in "$@"; do
    case "$arg" in -e*) test -f "${arg#-e}";; -L*) test -f "${arg#-L}";; esac
done
echo 'shlibs:Depends=libc6 (>= 2.34), libpcre2-8-0 (>= 10.39)'
''')
    executable(tools / "strip", "#!/bin/sh\nexit 0\n")
    executable(tools / "fakeroot", '#!/bin/sh\nexec "$@"\n')
    executable(tools / "dpkg-deb", '''#!/bin/sh
set -eu
test "$1" = --build
test -f "$2/DEBIAN/control"
test -f "$2/DEBIAN/shlibs"
tar -cf "$3" -C "$2" .
''')
    deb_env = dict(env, IVY_PACKAGE_TEST_LOG=str(temporary / "deb-make-calls"))
    package = temporary / "deb-output/libivy-test.deb"
    args = ["sh", str(source / "tools/package-deb.sh"), str(source),
            str(temporary / "deb-build"), str(package), "3.16.1.git20261102.test",
            "arm64", "2", "Mon, 02 Nov 2026 12:00:00 +0100"]
    run(args, env=deb_env)
    import gzip
    import tarfile
    with tarfile.open(package) as archive:
        control = archive.extractfile("./DEBIAN/control").read().decode()
        assert "Version: 3.16.1.git20261102.test" in control
        assert "Architecture: arm64" in control
        assert "libpcre2-dev, libc6" in control and "libpcre2-8-0" in control
        assert "ivy-c (= 3.16.1.git20261102.test)" in control
        assert archive.getmember("./usr/lib/libivy.so").linkname == "libivy.so.3.16"
        changelog = gzip.decompress(archive.extractfile("./usr/share/doc/libivy/changelog.gz").read()).decode()
        assert "3.16.1.git20261102.test" in changelog
    calls = (temporary / "deb-make-calls").read_text()
    assert "check" in calls and "PREFIX=/usr" in calls and "DESTDIR=" in calls
    assert (source / "src/stale.o").exists()
    assert not list((temporary / "deb-build").iterdir())
    failed_package = temporary / "deb-output/missing-dependencies.deb"
    args[4] = str(failed_package)
    run(args, env=dict(deb_env, IVY_PACKAGE_TEST_MISSING_DEPS="1"), success=False)
    assert not failed_package.exists()
    assert not list((temporary / "deb-build").iterdir())

print("Packaging recipes: SHA-256, Git metadata, MinGW staging and Debian dependency orchestration passed")
