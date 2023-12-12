#!/usr/bin/env -S pixi run python3

import argparse
import os
import sqlite3
import sys
import time
import xml.etree.ElementTree as ET
from dataclasses import asdict, astuple, dataclass
from pathlib import Path
import json
import random
from datetime import datetime, timedelta

from faker import Faker
from loguru import logger
from result import Err, Ok, Result, is_err, is_ok
import zmq


# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass

version_info = sys.version_info
if version_info.major < 3 or version_info.minor < 11:
    print("Python 3.11 or newer is required!")
    sys.exit(1)
import tomllib

def main() -> int:
    parser = argparse.ArgumentParser(prog=os.path.basename(__file__))

    args = parser.parse_args()
    print(f"{args = }")
    config_file_path = Path("config.toml")
    if not config_file_path.exists():
        logger.error(f"Cannot find `{config_file_path}`")
        return 1
    with open(config_file_path, "rb") as file:
        try:
            config = tomllib.load(file)
        except tomllib.TOMLDecodeError as e:
            logger.error(f"Cannot decode `{config_file_path}`: {e}")
            return 1

    port: int = config["server"]["streetlamps"]["port"]

    # print(f"{locals() = }")

    context = zmq.Context()
    client = context.socket(zmq.REQ)
    addr: str = f"tcp://localhost:{port}"
    client.connect(addr)
    logger.info(f"Connected to {addr}")

    id: int = 0
    try:
        while True:
            id += 1

            # {"jsonrpc": "2.0", "method": "lightlevel", "params": {"streetlamp": "123456", "reducer":
		    # "mean|median", "per": "quarter|hour|day|week", "from": "2020-01-01T00:00:00Z", "to":
		    # "2020-01-01T01:00:00Z"}, "id": 1}
            request= {
                "jsonrpc": "2.0",
                "method": "lightlevel",
                "params": {
                    "streetlamp": random.randint(0, 2**16),
                    "reducer": "mean",
                    "per": "hour",
                    "from": datetime.now().isoformat(),
                    "to": (datetime.now() + timedelta(hours=1)).isoformat(),
                },
                "id": id,
            }
            logger.info(f"Sending request: {request}")
            client.send(json.dumps(request).encode())
            reply = client.recv()
            print(f"Received reply: {reply}")
            time.sleep(0.5)
    except KeyboardInterrupt:
        logger.warning("Interrupted!")
    finally:
        client.close()
        context.term()

    return 0


    return 0


if __name__ == '__main__':
    sys.exit(main())
