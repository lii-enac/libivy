# libivy

Ivy software bus. The regular expression engine is **PCRE2, 8-bit API**
(`libpcre2-8` in pkg-config). The public Ivy C API is unchanged.

Install the build dependency:

```sh
# macOS
brew install pcre2 pkgconf
# Debian / Ubuntu
sudo apt install build-essential pkgconf libpcre2-dev
# MSYS2 / MinGW64
pacman -S mingw-w64-x86_64-pcre2 mingw-w64-x86_64-pkgconf
```

From the repository root or `src`, `make` selects the native platform backend.
`make help` lists the targets and `make clean` removes generated build files,
including test builds.

Build on macOS using the same targets as the Homebrew formula:

```sh
cd src
# Remove stale objects when migrating an existing checkout.
make -f Makefile.osx clean
make -f Makefile.osx static-libs shared-libs
make -f Makefile.osx install PREFIX=/your/install/prefix
```

The default build and packages include `ivyprobe` as their only command-line
tool. `ivyperf` (round-trip timing) and `ivythroughput` remain optional manual
build targets in the Unix tools Makefiles. They use Ivy's regex engine rather than
PCRE1's `pcrecpp` wrapper. For Linux use
`make` in `src`; for MinGW use `make -f Makefile.mingw`.
Use `CASESENSITIVE=no` to build a case-insensitive engine (default: `yes`).
Clean/rebuild existing objects when switching from PCRE1, changing this option,
or changing architecture (for example, Intel/Rosetta to Apple Silicon). Mixing
`x86_64` and `arm64` objects causes ignored-object warnings and undefined symbols.
To force a rebuild without cleaning, add `-B` to the build command.

Native MSVC Makefiles and the legacy `.vcproj` expect `PCRE2_ROOT` to point to
an installed PCRE2 with `include/pcre2.h` and `lib/pcre2-8.lib` (DLL import
library). The corresponding DLL must be available at runtime. The old `.sln`
references `ivy_c.vcxproj`, which is absent from this repository; it cannot be
built as supplied. Use the Makefile or regenerate the project for your toolchain.

## Tests

From the repository root or `src`:

```sh
make check
make check-caseless
make sanitize
```

The invalid-pattern test intentionally prints a compilation error, labelled as
expected; a successful run ends with `passed`. `make test` is an alias for
`make check`.

These tests need no network. They exercise matching, byte strings, named and
numbered captures, optional/empty groups, 300 captures, repeated matches and
compilation errors, runtime errors and repeated filter setup/termination.
The sanitizer target requires a compiler supporting
AddressSanitizer and UndefinedBehaviorSanitizer.

After building the library, test actual exchanges between two Ivy processes:

```sh
make check-bus
```

This test requires Python 3 and permission to open sockets. It uses a multicast
group and a dynamically selected UDP port; `IVY_TEST_ADDRESS` overrides the group.
It checks captures (including an absent optional group) and an acknowledgement.
To compare distinct static/shared or PCRE1/PCRE2 builds of `tests/bus.c`:

```sh
python3 tests/bus.py /path/to/sender-build /path/to/receiver-build
```

## Migration notes

The engine retains byte matching, case sensitivity, capture numbering and the
return convention of `IvyBindingExec`. UTF/UCP and JIT are not enabled implicitly.
Unset groups return length zero and a null pointer; empty participating groups
return a pointer with length zero. PCRE2 error wording can differ from PCRE1.

PCRE2 is not fully equivalent to PCRE1 for every pattern. Applications using
unusual constructs should test their own regex corpus; see the
[PCRE2 API](https://www.pcre.org/current/doc/html/pcre2api.html) and
[pattern documentation](https://www.pcre.org/current/doc/html/pcre2pattern.html).

Protocol reception accepts at most `MAX_MATCHING_ARGS` arguments (currently 120)
per message. Messages exceeding that limit are rejected in full, without calling
the application callback; the connection remains usable. `make check` and
`make sanitize` test this limit, empty arguments and reception after rejection,
without opening network sockets.

The send FIFO retains unsent data on `EAGAIN`/`EWOULDBLOCK` and retries `EINTR`.
Only bytes actually sent are removed. Fatal errors (or a zero-byte send with
pending data) close the connection rather than reporting successful delivery.
`IvyFifoSendSocket` returns the remaining byte count, or `IVY_FIFO_SEND_ERROR`
on failure. Tests simulate congestion, interruptions, partial writes and ring
buffer wrap without network sockets.

## Release version

Edit only `IVYMAJOR_VERSION`, `IVYMINOR_VERSION` and `IVYPATCH_VERSION` in
`src/version.h` to change the library release version. Linux, macOS and MinGW
Makefiles derive `MAJOR`, `MINOR`, `MINOR2` and `VERSION` from that header;
the package templates and pkg-config files use the same full version.
`make version` prints it from either the repository root or `src`.
The shared-library filename and SONAME retain their existing major/minor format.
The historical Debian changelog still tracks Debian package releases separately.

## Debian packages

Run on Debian/Ubuntu, from the repository root or `src`:

```sh
sudo apt install build-essential pkgconf libpcre2-dev dpkg-dev fakeroot
make deb
make deb_verify
sudo apt install ./build/deb/libivy_*.deb
```

`make deb` packages the current working tree with the release version from
`version.h`. It builds and tests a fresh source copy under `build/deb-build`,
installs into a staging directory under `/usr`, then builds a `.deb` in
`build/deb`. It never installs into the host system. The package includes shared
and static libraries, headers, pkg-config metadata, `ivyprobe`.
`dpkg-shlibdeps` determines runtime dependencies; `libpcre2-dev` and `pkgconf`
are also required because this package includes development files. The package
replaces the old `ivy-c`/`ivy-c-dev` packages.

For a development package from the same local working tree:

```sh
make deb_git
make deb_verify deb_package=build/deb/libivy_<version>.git<date>.<commit>_<arch>.deb
```

The development suffix belongs to the package version; the library version in
`version.h` is unchanged. `deb_arch` defaults to `dpkg --print-architecture`.
`deb_make_jobs`, `deb_build_dir`, `deb_package`, `deb_git_build_dir` and
`deb_git_package` can be overridden. Install `lintian` to enable its checks in
`deb_verify`. Relative path overrides are resolved from the repository root, including when
invoking make from `src`.

## MSYS2 / pacman packages

Use an MSYS2 MinGW64 shell with `makepkg`, `make`, Git and the MinGW64 GCC,
PCRE2 and pkgconf packages installed. All installed files use `/mingw64`:
`bin/libivy.dll`, `lib/libivy.dll.a`, `lib/libivy.a`, headers, pkg-config metadata
and `ivyprobe.exe`. No files are installed under MSYS2's `/usr` prefix.

From the repository root, build with `make -j8` (or `make -C src -j8`).
`src/Makefile` selects `Makefile.mingw` automatically: this is the MSYS2 native
Windows backend, also used by the older pacman recipe. The Linux backend uses
Unix socket/linker conventions and is unsuitable for MinGW DLLs. Compiler flags
can be overridden with `CFLAGS` while Windows/PCRE definitions stay in
`CPPFLAGS`. Only build settings are exported to recursive tool builds; package
metadata commands are evaluated when packaging needs them.

For a release archive:

```sh
make pkg
make pkg_verify
pacman -U build/libivy-*.pkg.tar.*
```

`make pkg` downloads the release archive, computes its SHA-256 and inserts it
into the generated PKGBUILD automatically. The package uses that same local
archive, with a fresh extraction on each run. No checksum is maintained in the
Makefile or supplied manually. The archive, its source URL and the generated
PKGBUILD remain in the packaging build directory. A download or hashing failure
stops the process before makepkg. For a development branch:

```sh
make pkg_git
make pkg_verify pkg_package_glob='build/libivy-*.git-*.pkg.tar.*'
```

`pkg_git_branch` defaults to `master`; `pkg_git_source` and `pkg_git_source_dir`
can also be overridden. Release builds accept `pkg_source`, `pkg_source_dir`
including a local source archive for validation before
publishing a release. The selected source must contain this packaging system;
older published archives are not patched for compatibility. Git builds use the
latest pushed branch, not uncommitted local changes.

Both variants use fresh build trees, run the non-network tests via `make check`,
and preserve the static library with makepkg's `staticlibs` option. PKGBUILDs
and source/build files are kept in `build/pkg-build` or `build/pkg-build-git`;
resulting packages are written to `build/`. `pkg_prefix` defaults to `/mingw64`,
and `pkg_make_jobs` controls parallelism.

`make clean_packages` removes generated Debian/pacman packages and their build
directories; ordinary `make clean` keeps package outputs.

For portable recipe checks, run `make check-packaging` (Python 3 required). These
use fixture compiler/package tools to verify staging, automatic release checksums,
Git source selection and Debian dependency generation/failure handling. They do
not produce installable packages or replace native Linux/MSYS2 validation.

## Jenkins and macOS static analysis

Build, packaging and analysis scripts live under `distrib/script/`.

`Jenkinsfile` checks out `https://github.com/lii-enac/libivy.git` anonymously.
The initial `master` checkout selects a commit; all other checkouts use that
exact commit. Configure the Jenkins job itself with this public URL and no Git
credentials. SCM polling checks for changes every five minutes; manual builds
work too. Each node uses a clean directory inside its Jenkins job workspace.

Every run first builds the static/shared libraries, command-line tools and
pkg-config metadata on `macos12`, then runs `make check`. The next stage runs
four branches in parallel:

- macOS: scan-build, then clang-tidy, with a separate fresh source copy for each;
- Ubuntu (`ubuntu`): build and `make check`;
- MSYS2/MinGW64 (`win10`): build and `make check`;
- WSL (`wsl`): build and `make check`.

`BUILD_ONLY` limits the other platform builds. The macOS build and both analyses
always run. A macOS compilation failure still allows these branches to run;
a scan-build failure still allows clang-tidy to run. Checkout failures and
agent allocation failures can prevent the dependent stages from starting.
There is no installation, package publication or downstream job trigger.

Required Jenkins components are Declarative Pipeline and Git; the Windows
agent uses the existing `Git-MSYS` Git installation. Install the normal build
dependencies on each agent. On macOS, also install Homebrew `llvm` and Python
3.9 or newer (`brew install llvm python`). Apple Clang builds the code; LLVM
provides the analyzers and stays out of `PATH`. MSYS2 needs GNU make and MinGW
GCC in addition to PCRE2/pkgconf, with MinGW executables available in `PATH`.

Both analyses cover the normal macOS C library and `ivyprobe`
(currently 10 C translation units). Optional event-loop backends and code guarded
by other platform macros are outside this analysis configuration. Clang-tidy
uses the compilation commands captured during a real successful build and
checks configured in `.clang-tidy`; it does not modify the sources.

Reports, logs, toolchain/commit metadata and a ZIP are archived as Jenkins
artifacts under `reports/scan-build`, `reports/tidy` and `analysis-reports.zip`.
Each report directory has an `index.html`; download the ZIP to browse locally
if Jenkins restricts HTML/CSS/JavaScript in artifacts. Findings are informational;
compilation failures, analyzer crashes and failed clang-tidy invocations fail
the job. A failed run retains an index explaining that the report is incomplete.
The Email Extension plugin sends a notification for each failed build/analysis
stage, followed by a final failure summary with artifact links. Ordinary successful
builds send no mail. Failure mails include the compressed console log. `EMAIL1` and `EMAIL2` override the default
recipients (Mathieu Poirier and Stéphane Conversy, as in djnn-cpp). A successful
build after a failure is marked `RECOVERED`. SMTP uses the Jenkins configuration.

Run the same analyses locally from the repository root or `src`:

```sh
make scan-build
make tidy
# Optional parallelism and report location (relative to the repository root):
make tidy tidy_jobs=4 tidy_report_dir=build/my-tidy-report
```

By default the reports are timestamped under `build/scan-build-report/` and
`build/tidy-report/`. An explicit report directory must be empty to prevent
mixing results from different runs. Isolated source copies and their build
files remain under `build/analysis/` for investigation.
