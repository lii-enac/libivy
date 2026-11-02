#!/usr/bin/env python3
"""Run a sender and receiver; accepts distinct builds to check interoperability."""
import os
import pathlib
import socket
import subprocess
import sys

sender = str(pathlib.Path(sys.argv[1]).resolve())
receiver = str(pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else sender).resolve())
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
bus = f"{os.environ.get('IVY_TEST_ADDRESS', '239.255.42.99')}:{port}"
processes = []
try:
    for executable, role in [(receiver, "receiver"), (sender, "sender")]:
        processes.append(subprocess.Popen(
            [executable, role, bus], stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True))
    failed = False
    for process in processes:
        output, _ = process.communicate(timeout=10)
        if output:
            print(output, end="")
        failed |= process.returncode != 0
    if failed:
        raise SystemExit("Ivy bus capture/ack test failed")
    print("Ivy bus: captures, optional group and acknowledgement passed")
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
        process.wait()
