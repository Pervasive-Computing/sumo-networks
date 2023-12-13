#!/usr/bin/env -S pixi run python3

import argparse
import os
import sqlite3
import sys
import xml.etree.ElementTree as ET
from dataclasses import astuple, dataclass
from pathlib import Path

from loguru import logger
from result import Err, Ok, Result, is_err, is_ok

# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass


@dataclass(frozen=True)
class StreetLamp:
    id: str
    lat: float
    lon: float


def parse_streetlamps_from_osm(osm_file_path: Path) -> Result[list[StreetLamp], str]:
    if not osm_file_path.exists():
        return Err(f"File {osm_file_path} does not exist")

    streetlamps: list[StreetLamp] = []
    # Example:
    # <node id='11329481598' timestamp='2023-11-06T15:50:34Z' uid='11444852' user='Jolesh' visible='true' version='1' changeset='143699517' lat='56.1738851' lon='10.1916587'>
    #     <tag k='highway' v='street_lamp' />
    #     <tag k='light:count' v='1' />
    #     <tag k='support' v='pole' />
    # </node>

    tree = ET.parse(osm_file_path)

    root = tree.getroot()
    nodes = root.findall("./node/tag[@k='highway'][@v='street_lamp']/..")
    for node in nodes:
        id = node.get("id")
        lat = float(node.get("lat"))
        lon = float(node.get("lon"))
        streetlamps.append(StreetLamp(id, lat, lon))

    return Ok(streetlamps)


def main() -> int:
    parser = argparse.ArgumentParser(prog=os.path.basename(__file__))
    parser.add_argument("osm_file_path", type=str, help="Path to the OSM file")
    parser.add_argument(
        "db_file_path",
        type=str,
        help="Path to the SQLite3 DB file",
        nargs="?",
        default="streetlamps.sqlite3",
    )

    args = parser.parse_args()
    print(f"{args = }")

    osm_file_path = Path(args.osm_file_path)

    db = sqlite3.connect(args.db_file_path)
    cursor = db.cursor()
    # Read sql statements from setup file and execute them
    with open("create-streetlamps-db.sql") as f:
        sql = f.read()
        cursor.executescript(sql)

    streetlamps_result = parse_streetlamps_from_osm(osm_file_path)
    if is_err(streetlamps_result):
        logger.error(
            f"Error parsing streetlamps from OSM file: {streetlamps_result.err}"
        )
        print(f"{streetlamps_result = }")
        return 1

    streetlamps = streetlamps_result.unwrap()
    # match parse_streetlamps_from_osm(osm_file_path):
    #     case Ok(streetlamps):
    #         print(f"{len(streetlamps)  = }")
    #     case Err(err):
    #         print(f"{err = }")
    #         return 1

    # Insert streetlamps into database
    cursor.executemany(
        "INSERT INTO streetlamps VALUES (?, ?, ?)",
        [astuple(streetlamp) for streetlamp in streetlamps],
    )
    db.commit()

    db.close()
    logger.info(f"Inserted {len(streetlamps) = } streetlamps into {args.db_file_path = }")
    return 0


if __name__ == "__main__":
    sys.exit(main())
