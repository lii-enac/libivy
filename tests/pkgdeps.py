#!/usr/bin/env python3
"""Check native dependency installation without changing the host packages."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "distrib/script/install-pkgdeps.sh"
MAKE = shutil.which("make")


def executable(path, text):
    path.write_text(text)
    path.chmod(0o755)


with tempfile.TemporaryDirectory(prefix="ivy-pkgdeps-") as directory:
    tools = Path(directory) / "bin"
    tools.mkdir()
    log = Path(directory) / "commands"
    executable(tools / "uname", '#!/bin/sh\nprintf "%s\\n" "$IVY_TEST_PLATFORM"\n')
    executable(tools / "id", '#!/bin/sh\nprintf "%s\\n" "$IVY_TEST_UID"\n')
    executable(tools / "sudo", '#!/bin/sh\nprintf "sudo\\n" >> "$IVY_TEST_LOG"\n"$@"\n')
    fixture = '''#!/bin/sh
manager=${0##*/}
case "$manager:$1" in
    dpkg-query:*|brew:list|pacman:-Q)
        for package do :; done
        case " ${IVY_TEST_INSTALLED:-} " in
            *" $package "*)
                if [ "$manager" = dpkg-query ]; then printf 'install ok installed'; fi
                exit 0 ;;
            *) exit 1 ;;
        esac
        ;;
esac
printf '%s' "$manager" >> "$IVY_TEST_LOG"
printf ' %s' "$@" >> "$IVY_TEST_LOG"
printf '\\n' >> "$IVY_TEST_LOG"
exit "${IVY_TEST_EXIT:-0}"
'''
    for name in ("brew", "apt", "pacman", "dpkg-query"):
        executable(tools / name, fixture)
    cases = [
        ("Linux", "apt", "build-essential pkgconf libpcre2-dev"),
        ("Darwin", "brew", "pcre2 pkgconf"),
        ("MINGW64_NT", "pacman", "make mingw-w64-x86_64-gcc mingw-w64-x86_64-pcre2 mingw-w64-x86_64-pkgconf"),
        ("MSYS_NT", "pacman", "make mingw-w64-x86_64-gcc mingw-w64-x86_64-pcre2 mingw-w64-x86_64-pkgconf"),
    ]
    for platform, manager, packages in cases:
        env = dict(os.environ, PATH=str(tools), IVY_TEST_PLATFORM=platform,
                   IVY_TEST_UID="1000", IVY_TEST_LOG=str(log))
        for installed in ("", packages.split()[0], packages):
            log.write_text("")
            result = subprocess.run(["/bin/sh", str(SCRIPT), "install"],
                                    env=dict(env, IVY_TEST_INSTALLED=installed),
                                    capture_output=True, text=True)
            assert result.returncode == 0, result.stdout + result.stderr
            lines = log.read_text().splitlines()
            assert ("sudo" in lines) == (platform == "Linux"), lines
            command = lines[-1].split()
            assert command[0] == manager
            assert command[-len(packages.split()):] == packages.split(), command
            assert "reinstall" not in command
            if manager == "apt": assert "--no-reinstall" in command
            if manager == "pacman":
                assert "--needed" in command and "--noconfirm" in command and "-S" in command
                assert "-Syu" not in command and "-Sy" not in command
        # Upgrade is explicit, includes all dependencies and propagates errors.
        log.write_text("")
        result = subprocess.run(["/bin/sh", str(SCRIPT), "upgrade"],
                                env=dict(env, IVY_TEST_UID="0", IVY_TEST_INSTALLED=packages,
                                         IVY_TEST_EXIT="42"))
        assert result.returncode == 42
        lines = log.read_text().splitlines()
        assert "sudo" not in lines
        assert lines[-1].endswith(packages), lines
        if manager == "apt": assert "--only-upgrade" in lines[-1]
        if manager == "pacman": assert "-Syu --needed" in lines[-1]
        if manager == "brew": assert lines[-1].startswith("brew upgrade ")

    result = subprocess.run(["/bin/sh", str(SCRIPT), "install"],
                            env=dict(env, IVY_TEST_PLATFORM="UnknownOS"),
                            capture_output=True, text=True)
    assert result.returncode != 0 and "Supported systems" in result.stderr

    # Exercise real Makefiles from both the repository root and src.
    executable(tools / "pkg-config", "#!/bin/sh\nexit 0\n")
    for platform, manager, packages in cases:
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"],
                   IVY_TEST_PLATFORM=platform, IVY_TEST_UID="0", IVY_TEST_LOG=str(log))
        for cwd in (ROOT, ROOT / "src"):
            for target in ("install-pkgdeps", "upgrade-pkgdeps"):
                log.write_text("")
                result = subprocess.run([MAKE, "-s", target], cwd=cwd, env=env,
                                        capture_output=True, text=True)
                assert result.returncode == 0, result.stdout + result.stderr
                assert any(line.startswith(manager + " ") for line in log.read_text().splitlines())

print("Dependency installers: APT/Homebrew/pacman, update/no-reinstall commands, failures and root/src targets passed")
