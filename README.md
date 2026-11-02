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

The install target also builds the tools, including `ivythroughput`. The tools
now use Ivy's regex engine rather than PCRE1's `pcrecpp` wrapper. For Linux use
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

Release packaging and the Homebrew formula still need a coordinated update to
PCRE2 before publishing this migration. In particular, the existing Homebrew
formula fetches the old 3.15.8 release and depends on `pcre`.

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
