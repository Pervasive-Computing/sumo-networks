#!/usr/bin/env -S pixi run python3

import argparse
import os
import subprocess
import sys
import time
import random

# import time
# from dataclasses import asdict, dataclass
# from datetime import datetime
from pathlib import Path
from shutil import which

version_info = sys.version_info
if version_info.major < 3 or version_info.minor < 11:
    print("Python 3.11 or newer is required!")
    sys.exit(1)
import tomllib


import zmq
import cbor2
from loguru import logger
# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass


def main() -> int:
    parser = argparse.ArgumentParser(
        prog=os.path.basename(__file__).removesuffix(".py"),
    )
    parser.add_argument('-p', '--port', type=int, required=True, help='Port to connect to')

    args = parser.parse_args()

    context = zmq.Context()
    client = context.socket(zmq.REQ)
    addr: str = f"tcp://localhost:{args.port}"
    client.connect(addr)
    logger.info(f"Connected to {addr}")

    try:
        while True:
            streetlamp_id: int = random.randint(0, 100)
            request_type: str = "daily" if random.randint(0, 1) == 0 else "weekly"
            msg: bytes = b"/".join([str(streetlamp_id).encode(), request_type.encode()])
            client.send(msg)
            reply = client.recv()
            print(f"Received reply: {reply}")
            time.sleep(0.5)
    except KeyboardInterrupt:
        logger.warning("Interrupted!")
    finally:
        client.close()
        context.term()

    return 0

if __name__ == "__main__":
    sys.exit(main())
