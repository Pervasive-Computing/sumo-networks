#!/usr/bin/env -S pixi run python3

import argparse
import os
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from shutil import which

version_info = sys.version_info
if version_info.major < 3 or version_info.minor < 11:
    print("Python 3.11 or newer is required!")
    sys.exit(1)
import tomllib

# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass


def main(argc: int, argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        prog=os.path.basename(__file__).removesuffix(".py"),
        description="Run SUMO simulation and publish data",
    )
    parser.add_argument(
        "-g", "--gui", action="store_true", help="Use `sumo-gui` instead of `sumo`"
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose output"
    )
    args = parser.parse_args(argv[1:])

    sumo_cmd: str = "sumo-gui" if args.gui else "sumo"
    sumo_cmd_path = which(sumo_cmd)
    if sumo_cmd_path is None:
        print(f"Cannot find `{sumo_cmd}` in PATH!", file=sys.stderr)
        return 1

    configuration_path = Path("configuration.toml")
    if not configuration_path.exists():
        print(f"Cannot find `{configuration_path}`!")
        return 1
    with open(configuration_path, "rb") as file:
        try:
            configuration = tomllib.load(file)
        except tomllib.TOMLDecodeError as e:
            print(f"Cannot decode `{configuration_path}`: {e}")
            return 1

    sumo_sim_data_publisher_path = Path.cwd() / "build" / "sumo-sim-data-publisher"
    if not sumo_sim_data_publisher_path.exists():
        print(f"Cannot find `{sumo_sim_data_publisher_path}`!")
        print("Did you forget to build the project?")
        return 1

    # zellij_available: bool = which("zellij") is not None
    # inside_zellij: bool = os.environ.get("ZELLIJ") is not None

    print("[1/2] Starting simulation")
    sumo_cmd_args: list[str] = list(
        map(
            str,
            [
                sumo_cmd_path,
                "-c",
                configuration["sumo"]["sumocfg-path"],
                "--remote-port",
                str(configuration["sumo"]["port"]),
            ],
        )
    )

    if args.verbose:
        print(f"{sumo_cmd_args = }")

    p = subprocess.Popen(sumo_cmd_args)

    print("[2/2] Starting simulation data publisher")
    sumo_sim_data_publisher_args: list[str] = list(
        map(str, [sumo_sim_data_publisher_path])
    )

    if args.verbose:
        print(f"{sumo_sim_data_publisher_args = }")

    subprocess.run(sumo_sim_data_publisher_args)

    p.wait()

    return 0


if __name__ == "__main__":
    sys.exit(main(len(sys.argv), sys.argv))
