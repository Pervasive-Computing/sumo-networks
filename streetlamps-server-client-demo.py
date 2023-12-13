#!/usr/bin/env -S pixi run python3

import argparse
import json
import os
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path

import zmq
from loguru import logger

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
    parser.add_argument(
        "-s", "--streetlamp-id", type=int, help="Streetlamp ID", default=-26043
    )
    parser.add_argument(
        "-r",
        "--reducer",
        type=str,
        help="Reducer",
        default="mean",
        choices=["mean", "median"],
    )
    parser.add_argument(
        "-p",
        "--per",
        type=str,
        help="Per",
        default="hour",
        choices=["quarter", "hour", "day", "week"],
    )

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
            # Expect a json-rpc 2.0 request
            # {"jsonrpc": "2.0", "method": "lightlevel", "params": {"streetlamp": "123456", "reducer":
            # "mean|median", "per": "quarter|hour|day|week", "from": "1702396387", "to":
            # "1701791732"}, "id": 1}
            now = datetime.now()
            to_unix_ts = int(now.timestamp())
            from_unix_ts = int((now - timedelta(days=1)).timestamp())

            request = {
                "jsonrpc": "2.0",
                "method": "lightlevel",
                "params": {
                    "streetlamp": args.streetlamp_id,
                    "reducer": args.reducer,
                    "per": args.per,
                    "from": from_unix_ts,
                    "to": to_unix_ts,
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


if __name__ == "__main__":
    sys.exit(main())
