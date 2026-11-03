#!/usr/bin/env python3
"""Run clang-tidy from a compilation database, retaining logs and an HTML index."""

import argparse
import concurrent.futures
import datetime
import html
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parent.parent
DIAGNOSTIC = re.compile(r"^(.+?):(\d+):(\d+): (warning|error|fatal error): (.+)$")


def find_tidy(explicit):
    if explicit:
        candidate = shutil.which(explicit)
        if candidate:
            return candidate
        raise RuntimeError("clang-tidy executable not found: " + explicit)
    brew = shutil.which("brew")
    if brew:
        result = subprocess.run([brew, "--prefix", "llvm"], capture_output=True, text=True)
        candidate = Path(result.stdout.strip()) / "bin/clang-tidy"
        if result.returncode == 0 and os.access(candidate, os.X_OK):
            return str(candidate)
    candidate = shutil.which("clang-tidy")
    if candidate:
        return candidate
    raise RuntimeError("Install LLVM or pass --clang-tidy=/path/to/clang-tidy.")


def source_files(database, selectors):
    files = set()
    for entry in database:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        path = path.resolve()
        if not any(path.is_relative_to(ROOT / folder) for folder in ("src", "tools")):
            continue
        # Analyze maintained sources; generated/third-party code adds noise.
        if {"ext", "gen"}.intersection(path.relative_to(ROOT).parts):
            continue
        if path.suffix not in {".c", ".cpp", ".cc", ".cxx"}:
            continue
        if selectors and not any(path == selector or selector in path.parents for selector in selectors):
            continue
        files.add(path)
    return sorted(files)


def parse_diagnostics(output, directory):
    diagnostics = []
    for line in output.splitlines():
        match = DIAGNOSTIC.match(line)
        if not match:
            continue
        filename, row, column, severity, message = match.groups()
        check_match = re.search(r" \[([^\]]+)\]$", message)
        check = check_match.group(1) if check_match else "compiler"
        if check_match:
            message = message[:check_match.start()]
        path = Path(filename)
        if not path.is_absolute():
            path = Path(directory) / path
        path = path.resolve()
        filename = path.relative_to(ROOT).as_posix() if path.is_relative_to(ROOT) else str(path)
        diagnostics.append(dict(file=filename, line=int(row), column=int(column),
                                severity=severity, message=message, check=check))
    return diagnostics


def analyze(binary, build_dir, report, source, env):
    name = source.relative_to(ROOT).as_posix().replace("/", "__")
    command = [binary, "-p=" + str(build_dir), "--config-file=" + str(ROOT / ".clang-tidy"),
               "--use-color=false", "--export-fixes=" + str(report / (name + ".yaml")), str(source)]
    start = time.monotonic()
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True)
    output = result.stdout + result.stderr
    (report / (name + ".log")).write_text(output, encoding="utf-8")
    return dict(source=source.relative_to(ROOT).as_posix(), returncode=result.returncode,
                seconds=round(time.monotonic() - start, 2), log=name + ".log",
                command=command, diagnostics=parse_diagnostics(output, ROOT))


def write_report(report, results, version, started):
    # Headers can appear in several translation units: show each diagnostic once.
    unique = {}
    for result in results:
        for diagnostic in result["diagnostics"]:
            key = tuple(diagnostic.values())
            unique.setdefault(key, dict(diagnostic, log=result["log"]))
    diagnostics = sorted(unique.values(), key=lambda d: (d["file"], d["line"], d["check"]))
    failures = sum(result["returncode"] != 0 for result in results)
    summary = dict(started=started, tool=version, files=len(results), failed_files=failures,
                   diagnostics=diagnostics, runs=results)
    (report / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    escape = html.escape
    rows = []
    for diagnostic in diagnostics:
        location = f'{diagnostic["file"]}:{diagnostic["line"]}:{diagnostic["column"]}'
        rows.append('<tr><td>' + escape(location) + '</td><td>' + escape(diagnostic["check"]) +
                    '</td><td>' + escape(diagnostic["severity"]) + '</td><td>' +
                    escape(diagnostic["message"]) + '</td><td><a href="' +
                    escape(diagnostic["log"], quote=True) + '">Log</a></td></tr>')
    logs = ''.join('<li><a href="' + escape(result["log"], quote=True) + '">' +
                   escape(result["source"]) + '</a> — ' +
                   ("analysis failed" if result["returncode"] else "analyzed") +
                   f' ({result["seconds"]} s)</li>' for result in results)
    status = "Analysis incomplete" if failures else "Analysis completed"
    document = '''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>libivy · clang-tidy</title><style>
body{font:15px system-ui,sans-serif;color:#1e293b;background:#f1f5f9;margin:0;padding:32px}
main{max-width:1450px;margin:auto}h1{margin-bottom:6px}p{color:#475569}
.stats{display:flex;flex-wrap:wrap;gap:16px;margin:24px 0}.stats div{background:white;
padding:18px 24px;border-radius:10px;border:1px solid #cbd5e1}.stats b{font-size:24px;display:block}
input{padding:12px;box-sizing:border-box;width:100%;border:1px solid #94a3b8;border-radius:6px}
.table{overflow:auto;margin:16px 0}table{width:100%;border-collapse:collapse;background:white}
th,td{padding:12px;text-align:left;border-bottom:1px solid #e2e8f0;vertical-align:top}
th{background:#e2e8f0}td:first-child,td:nth-child(2){font-family:monospace;font-size:13px}
a{color:#0369a1}details{background:white;padding:16px;border-radius:8px}li{margin:7px 0}
</style><main><h1>libivy · clang-tidy</h1>'''
    document += '<p>' + escape(status + ' · ' + started + ' · ' + version) + '</p>'
    document += '<div class="stats">' + ''.join('<div><b>' + str(count) + '</b>' + label + '</div>'
                  for count, label in [(len(results), "files analyzed"), (len(diagnostics), "unique diagnostics"),
                                       (failures, "failed analyses")]) + '</div>'
    document += '<p>Potential issues, not confirmed bugs. No automatic fixes were applied. '
    document += 'Generated and third-party sources are excluded. Open the logs for diagnostic notes.</p>'
    document += '<input id="search" type="search" aria-label="Filter diagnostics" placeholder="Filter by file, check or message…">'
    document += '<div class="table"><table><thead><tr><th>Location</th><th>Check</th><th>Severity</th>'
    document += '<th>Message</th><th>Details</th></tr></thead><tbody>' + ''.join(rows) + '</tbody></table></div>'
    if not diagnostics:
        document += '<p>No diagnostics were parsed. Check analysis failures and logs before concluding.</p>'
    document += '<details><summary>All analysis logs</summary><ul>' + logs + '</ul></details>'
    document += '''<script>document.getElementById('search').addEventListener('input',function(){
const query=this.value.toLowerCase();document.querySelectorAll('tbody tr').forEach(function(row){
row.hidden=!row.textContent.toLowerCase().includes(query);});});</script></main></html>'''
    (report / "index.html").write_text(document, encoding="utf-8")
    return failures, len(diagnostics)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--report-dir", required=True)
    parser.add_argument("--clang-tidy")
    parser.add_argument("--files", default="", help="Space-separated source files/directories")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    binary = find_tidy(args.clang_tidy)
    build_dir = Path(args.build_dir).resolve()
    database = json.loads((build_dir / "compile_commands.json").read_text())
    selectors = [(ROOT / item).resolve() for item in shlex.split(args.files)]
    sources = source_files(database, selectors)
    if not sources:
        raise RuntimeError("No sources selected from compile_commands.json; check --files and build configuration.")
    env = os.environ.copy()
    if sys.platform == "darwin":
        env["SDKROOT"] = subprocess.check_output(
            ["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip()
    version = subprocess.check_output([binary, "--version"], text=True).strip().replace("\n", " ")
    now = datetime.datetime.now().astimezone()
    report = Path(args.report_dir).resolve() / now.strftime("%Y-%m-%d-%H%M%S-%f")
    report.mkdir(parents=True)
    shutil.copyfile(ROOT / ".clang-tidy", report / "checks.yaml")
    results = []
    print(f"{version}\nAnalyzing {len(sources)} files with {args.jobs} workers. LLVM stays out of PATH.", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        tasks = [pool.submit(analyze, binary, build_dir, report, source, env) for source in sources]
        for task in concurrent.futures.as_completed(tasks):
            result = task.result()
            results.append(result)
            status = "FAILED" if result["returncode"] else "OK"
            print(f'[{len(results)}/{len(sources)}] {status} {result["source"]}', flush=True)
    results.sort(key=lambda result: result["source"])
    failures, count = write_report(report, results, version, now.isoformat(timespec="seconds"))
    print(f"{count} unique diagnostics; {failures} failed analyses.\nReport: {report / 'index.html'}", flush=True)
    # Findings are informational; compiler/tool failures must fail make/Jenkins.
    return 1 if failures else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print("clang-tidy: " + str(error), file=sys.stderr)
        sys.exit(1)
