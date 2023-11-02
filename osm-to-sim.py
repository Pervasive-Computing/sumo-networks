#!/usr/bin/env python3

import os
import sys
import argparse
import re
from pathlib import Path
from datetime import datetime
import shutil
import subprocess

# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass

class TermColors:
    RESET = "\033[0m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"


def hr(char: str = "-", length: int | None = None) -> None:
    if length is None:
        length = os.get_terminal_size().columns
    print(char * length)


_steps: int | None = None
_total_steps: int | None = None
def step(msg: str) -> None:
    global _steps, _total_steps

    if _steps is None:
        _steps = 0
    if _total_steps is None:
        _total_steps = 0
        # Search through the current script for all calls to step()
        regexp = re.compile(r"step\(\s*(?P<msg>.*)\s*\)")
        for line in Path(__file__).read_text().splitlines():
            match = regexp.match(line)
            if match:
                _total_steps += 1
    _steps += 1
    # msg = f"[{TermColors.BLUE}{_steps}{TermColors.RESET}/{TermColors.CYAN}{_total_steps}{TermColors.RESET}] {msg}"
    msg = f"[{_steps}/{_total_steps}] {msg}"
    print(msg)

if "SUMO_HOME" not in os.environ:
    print("environment variable SUMO_HOME not set")
    sys.exit(1)

scriptname: str = os.path.basename(__file__).removesuffix(".py")
parser = argparse.ArgumentParser(prog=scriptname)
parser.add_argument("osm_file", type=str, help="input osm file")
parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
parser.add_argument("-d", "--dry-run", action="store_true", help="Dry run")

args = parser.parse_args()

if args.verbose:
    def verbose(*args, **kwargs) -> None:
        # print(datetime.now(), *args, **kwargs)
        print(*args, **kwargs)
else:
    def verbose(*args, **kwargs) -> None:
        pass


verbose(f"{sys.executable = }")

if not args.osm_file:
    parser.print_help()
    sys.exit(1)

osm_file = Path(args.osm_file)
assert osm_file.exists(), f"file {osm_file} does not exist"
osm_name: str = osm_file.stem
osm_dir: Path = osm_file.parent

def running_in_wsl() -> bool:
    """ Check if interpreter process is running in Windows Subsystem for Linux """
    try:
        with open('/proc/version', 'r') as f:
            if 'microsoft' in f.read().lower():
                return True
    except FileNotFoundError:
        pass
    return False

postfix: str = ".exe" if running_in_wsl() else ""
def exe(binary: str) -> str:
    return binary + postfix

sumo_binary: str | None = shutil.which(exe("sumo"))
assert sumo_binary is not None, "sumo binary not found in PATH"
sumo_gui_binary: str | None = shutil.which(exe("sumo-gui"))
assert sumo_gui_binary is not None, "sumo-gui binary not found in PATH"
netconvert_binary: str | None = shutil.which(exe("netconvert"))
assert netconvert_binary is not None, "netconvert binary not found in PATH"
polyconvert_binary: str | None = shutil.which(exe("polyconvert"))
assert polyconvert_binary is not None, "polyconvert binary not found in PATH"

os.chdir(osm_dir)

step("Creating SUMO network from OSM ...")

netconvert_args: list[str] = list(map(str, [
    netconvert_binary,
    "--osm-files",
    osm_file,
    "--output-file",
    osm_name + ".net.xml",
    "--junctions.join",
    "--no-left-connections",
    "--tls.discard-simple",
    "--tls.default-type",
    "actuated",
    "--no-turnarounds",
]))

verbose(f"{netconvert_args = }")

if not args.dry_run:
    result: subprocess.CompletedProcess = subprocess.run(netconvert_args)
    if result.returncode != 0:
        print(f"{result.stderr = }")
        sys.exit(1)


randomTrips_python_script = Path(f"{os.environ['SUMO_HOME']}/tools/randomTrips.py")
assert randomTrips_python_script.exists(), f"file {randomTrips_python_script} does not exist. Have you installed SUMO?"


step("Creating SUMO routes from OSM ...")
randomTrips_args: list[str] = list(map(str, [
    sys.executable,
    randomTrips_python_script,
    "-n",
    f"{osm_name}.net.xml",
    "--random-routing-factor",
    2.0,
    "--insertion-density",
    100,
    "-e",
    20000,
    "-L",
    "-r",
    f"{osm_name}.rou.xml",
]))

verbose(f"{randomTrips_args = }")

if not args.dry_run:
    result: subprocess.CompletedProcess = subprocess.run(randomTrips_args)
    if result.returncode != 0:
        print(f"{result.stderr = }")
        sys.exit(1)


typemap_path = Path("typemap.xml")
if not typemap_path.exists():
    print(f"typemap.xml not found in {os.getcwd() = }")
    print(f"Copying typemap.xml from {os.environ['SUMO_HOME']}/data/typemap/osmPolyconvert.typ.xml")
    if not args.dry_run:
        shutil.copyfile(f"{os.environ['SUMO_HOME']}/data/typemap/osmPolyconvert.typ.xml", "typemap.xml")


step("Creating SUMO polygons from OSM file")

polyconvert_args: list[str] = list(map(str, [
    polyconvert_binary,
    "--net-file",
    f"{osm_name}.net.xml",
    "--osm-files",
    osm_file,
    "--type-file",
    typemap_path,
    "-o",
    f"{osm_name}.poly.xml",
]))

verbose(f"{polyconvert_args = }")
if not args.dry_run:
    result: subprocess.CompletedProcess = subprocess.run(polyconvert_args)
    if result.returncode != 0:
        print(f"{result.stderr = }")
        sys.exit(1)


step(f"Creating SUMO configuration file at {osm_name}.sumocfg ...")

sumocfg: str = f"""<?xml version="1.0" encoding="UTF-8"?>
<configuration xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
               xsi:noNamespaceSchemaLocation="../xml-schemas/sumocfg.schema.xsd">

    <input>
        <net-file value="{osm_name}.net.xml"/>
        <route-files value="{osm_name}.rou.xml"/>
        <additional-files value="{osm_name}.poly.xml"/>
    </input>
    <time>
        <begin value="0"/>
        <step-length value="0.1"/>
        <end value="20000"/>
    </time>
    <gui_only>
        <delay value="0"/>
        <start value="true"/>
    </gui_only>
</configuration>
"""

if not args.dry_run:
    with open(f"{osm_name}.sumocfg", "w") as f:
        f.write(sumocfg)
 

print("Done :D")
