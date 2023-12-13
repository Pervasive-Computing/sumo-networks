#!/usr/bin/env -S pixi run python3

import argparse
import os
import random
import sqlite3
import sys
import time
import xml.etree.ElementTree as ET
from dataclasses import asdict, astuple, dataclass
from datetime import datetime, timedelta
from pathlib import Path

from faker import Faker
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


def main() -> int:
    parser = argparse.ArgumentParser(prog=os.path.basename(__file__))
    parser.add_argument(
        "db_file_path",
        type=str,
        help="Path to the SQLite3 DB file",
        nargs="?",
        default="streetlamps.sqlite3",
    )
    parser.add_argument(
        "-n", type=int, help="Number of measurements to generate", default=1000
    )
    parser.add_argument(
        "-s", "--streetlamp-id", type=str, help="Streetlamp ID", nargs="?", default=""
    )

    args = parser.parse_args()
    print(f"{args = }")

    db = sqlite3.connect(args.db_file_path)
    cursor = db.cursor()

    # Check that the tables 'streetlamps' and 'measurements' exist
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
    tables = cursor.fetchall()
    if ("streetlamps",) not in tables:
        logger.error(f"Table 'streetlamps' does not exist in {args.db_file_path = }")
        return 1
    if ("measurements",) not in tables:
        logger.error(f"Table 'measurements' does not exist in {args.db_file_path = }")
        return 1

    if args.n < 1:
        logger.error(f"{args.n = } must be >= 1")
        return 1

    streetlamp_ids: list[str] = []
    if args.streetlamp_id:
        cursor.execute("SELECT id FROM streetlamps WHERE id = ?", (args.streetlamp_id,))
        streetlamp_id = cursor.fetchone()
        if streetlamp_id is None:
            logger.error(f"Streetlamp with ID {args.streetlamp_id} does not exist")
            return 1
        streetlamp_ids.append(args.streetlamp_id)
    else:
        cursor.execute("SELECT id FROM streetlamps")
        streetlamp_ids = [id for (id,) in cursor.fetchall()]

    # end_datetime = datetime(2023, 12, 12)
    end_datetime = datetime.now()
    end_unix_timestamp = int(end_datetime.timestamp())
    # Generate random unix timestamps from `end_datetime` to `end_datetime - 1 week`
    start_datetime = end_datetime - timedelta(weeks=1)
    start_unix_timestamp = int(start_datetime.timestamp())

    logger.info(
        f"Generating {args.n = } measurements with timestamps between {start_datetime} and {end_datetime}..."
    )

    for i in range(args.n):
        timestamp = random.randint(start_unix_timestamp, end_unix_timestamp)
        # Generate random value between 0 and 1, for the light intensity
        light_intensity = random.random()
        assert 0 <= light_intensity <= 1, f"0 <= {light_intensity = } <= 1"
        # Choose a random streetlamp id
        streetlamp_id = random.choice(streetlamp_ids)
        cursor.execute(
            "INSERT INTO measurements (streetlamp_id, timestamp, light_level) VALUES (?, ?, ?)",
            (streetlamp_id, timestamp, light_intensity),
        )


    db.commit()
    logger.info("Done!")

    db.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
