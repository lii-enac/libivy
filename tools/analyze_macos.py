#!/usr/bin/env python3
"""Build a fresh libivy source copy and produce a macOS static-analysis report."""
import argparse
import html
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def output(command):
    return subprocess.check_output(command, text=True).strip()


def run(command, cwd, env, log):
    with log.open("w") as stream:
        stream.write(shlex.join([str(part) for part in command]) + "\n\n")
        stream.flush()
        result = subprocess.run(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError("Command failed; see " + str(log))


def write_index(report, mode, success, error=""):
    title = "libivy · " + mode
    links = []
    for path in sorted(report.rglob("index.html")):
        if path != report / "index.html":
            relative = path.relative_to(report).as_posix()
            links.append('<li><a href="' + html.escape(relative, quote=True) + '">' + html.escape(relative) + '</a></li>')
    for name in ("toolchain.json", "build.log", "tidy.log", "compile_commands.json"):
        if (report / name).exists():
            links.append('<li><a href="' + name + '">' + name + '</a></li>')
    status = "Completed" if success else "FAILED / incomplete"
    document = ('<!doctype html><html lang="en"><meta charset="utf-8"><title>' + title +
                '</title><body><h1>' + title + '</h1><p>' + status + '</p><p>' +
                html.escape(error) + '</p><ul>' + ''.join(links) + '</ul>' +
                '<p>Diagnostics are informational. Build/tool failures fail the job. '
                'Read the logs and coverage metadata even when no findings are reported.</p></body></html>')
    (report / "index.html").write_text(document)


def analyze(args, report):
    if sys.platform != "darwin":
        raise RuntimeError("Static analysis must run on macOS")
    llvm = Path(output(["brew", "--prefix", "llvm"]))
    cc = output(["/usr/bin/xcrun", "--find", "clang"])
    cxx = output(["/usr/bin/xcrun", "--find", "clang++"])
    sdk = output(["/usr/bin/xcrun", "--sdk", "macosx", "--show-sdk-path"])
    if "Apple clang" not in output([cc, "--version"]):
        raise RuntimeError("Expected Apple clang for the build")
    env = dict(os.environ, SDKROOT=sdk)
    metadata = {"revision": output(["git", "-C", str(ROOT), "rev-parse", "HEAD"]),
                "mode": args.mode, "cc": cc, "cxx": cxx, "sdk": sdk,
                "compiler": output([cc, "--version"]), "llvm": str(llvm)}
    (report / "toolchain.json").write_text(json.dumps(metadata, indent=2) + "\n")
    work_parent = ROOT / "build/analysis"
    work_parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=args.mode + "-", dir=work_parent))
    source = work / "source"
    shutil.copytree(ROOT, source, ignore=shutil.ignore_patterns(
        ".git", ".vscode", "build", "build-*", "reports", "__pycache__", "*.pyc",
        "*.o", "*.a", "*.so*", "*.dylib*", "*.dll*", "*.exe", "*.pc",
        "ivyprobe", "ivyperf", "ivythroughput"))
    goals = ["static-libs", "shared-libs", "tools", "pkgconf"]
    if args.mode == "scan-build":
        scan = llvm / "bin/scan-build"
        analyzer = llvm / "bin/clang"
        cc_wrapper = llvm / "libexec/ccc-analyzer"
        cxx_wrapper = llvm / "libexec/c++-analyzer"
        for binary in (scan, analyzer, cc_wrapper, cxx_wrapper):
            if not os.access(binary, os.X_OK):
                raise RuntimeError("Missing LLVM tool: " + str(binary))
        command = [str(scan), "--use-analyzer", str(analyzer), "--use-cc", cc,
                   "--use-c++", cxx, "--keep-empty", "-o", str(report / "html"),
                   "/usr/bin/make", "-j" + str(args.jobs)] + goals + [
                   "CC=" + shlex.quote(str(cc_wrapper)), "CPP=" + shlex.quote(str(cxx_wrapper)),
                   "CXX=" + shlex.quote(str(cxx_wrapper))]
        # Override Makefile CC/CPP explicitly; environment-only wrappers are ignored.
        run(command, source, env, report / "build.log")
        failures = [path for path in (report / "html").rglob("*.info.txt")
                    if path.parent.name == "failures"]
        if failures:
            raise RuntimeError("scan-build analyzer failures: " + str(len(failures)))
    else:
        capture = source / "tools/capture_compile.py"
        commands = work / "commands"
        env.update(IVY_ANALYSIS_CC=cc, IVY_ANALYSIS_CXX=cxx, IVY_ANALYSIS_COMMANDS=str(commands))
        wrapper = shlex.quote(sys.executable) + " " + shlex.quote(str(capture))
        run(["/usr/bin/make", "-j" + str(args.jobs)] + goals + [
            "CC=" + wrapper + " c", "CPP=" + wrapper + " cxx", "CXX=" + wrapper + " cxx"],
            source, env, report / "build.log")
        database = [json.loads(path.read_text()) for path in sorted(commands.glob("*.json"))]
        covered = {Path(entry["file"]).relative_to(source).as_posix() for entry in database}
        required = {"src/ivy.c", "src/ivybind.c", "src/ivysocket.c", "src/ivyfifo.c"}
        if not required.issubset(covered):
            raise RuntimeError("Missing compilation commands: " + repr(sorted(required - covered)))
        database.sort(key=lambda entry: entry["file"])
        (work / "compile_commands.json").write_text(json.dumps(database, indent=2) + "\n")
        shutil.copyfile(work / "compile_commands.json", report / "compile_commands.json")
        metadata["sources"] = sorted(covered)
        (report / "toolchain.json").write_text(json.dumps(metadata, indent=2) + "\n")
        run([sys.executable, str(source / "tools/run_tidy.py"), "--build-dir", str(work),
             "--report-dir", str(report / "html"), "--jobs", str(args.jobs),
             "--clang-tidy", str(llvm / "bin/clang-tidy")], source, env, report / "tidy.log")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["scan-build", "tidy"])
    parser.add_argument("--report-dir", required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    report = Path(args.report_dir).resolve()
    report.mkdir(parents=True, exist_ok=True)
    if any(report.iterdir()):
        parser.error("Use a new empty report directory to avoid mixing analysis runs")
    try:
        analyze(args, report)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        write_index(report, args.mode, False, str(error))
        print(error, file=sys.stderr)
        return 1
    write_index(report, args.mode, True)
    print("Report: " + str(report / "index.html"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
