#!/usr/bin/env python3
"""Record actual successful Apple compiler invocations for clang-tidy."""
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid


def main():
    kind = sys.argv[1]
    compiler = os.environ["IVY_ANALYSIS_CXX" if kind == "cxx" else "IVY_ANALYSIS_CC"]
    arguments = [compiler, "-isysroot", os.environ["SDKROOT"]] + sys.argv[2:]
    result = subprocess.run(arguments)
    if result.returncode == 0 and "-c" in arguments:
        sources = [argument for argument in arguments[1:]
                   if not argument.startswith("-") and Path(argument).suffix in {".c", ".cpp", ".cc", ".cxx"}]
        if len(sources) != 1:
            raise RuntimeError("Expected one source per compiler invocation: " + repr(arguments))
        entry = {"directory": str(Path.cwd()), "file": str(Path(sources[0]).resolve()),
                 "arguments": arguments}
        destination = Path(os.environ["IVY_ANALYSIS_COMMANDS"])
        destination.mkdir(parents=True, exist_ok=True)
        (destination / (uuid.uuid4().hex + ".json")).write_text(json.dumps(entry))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
