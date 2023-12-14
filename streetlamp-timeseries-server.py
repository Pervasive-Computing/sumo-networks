#!/usr/bin/env -S pixi run python3

import argparse
import os
import sqlite3
import sys
from dataclasses import asdict, astuple, dataclass
from datetime import datetime, timedelta
from pathlib import Path
from typing import Protocol

import tomllib
from flask import Flask, Response, g, jsonify, request
from loguru import logger

if "FLASK_ENV" not in os.environ:
    os.environ["FLASK_ENV"] = "production"


CONFIG_FILE_PATH = Path(__file__).parent / "config.toml"
if not CONFIG_FILE_PATH.exists():
    logger.error(f"Cannot find `{CONFIG_FILE_PATH}`!")
    sys.exit(1)
with open(CONFIG_FILE_PATH, "rb") as file:
    try:
        config = tomllib.load(file)
    except tomllib.TOMLDecodeError as e:
        logger.error(f"Cannot decode `{CONFIG_FILE_PATH}`: {e}")
        sys.exit(1)

argv_parser = argparse.ArgumentParser(prog=os.path.basename(__file__))
argv_parser.add_argument(
    "-v", "--verbose", action="store_true", help="Enable verbose output"
)
argv_parser.add_argument(
    "-d", "--debug", action="store_true", help="Enable debug output"
)

args = argv_parser.parse_args(sys.argv[1:])

if args.verbose:
    logger.remove()
    logger.add(sys.stderr, level="INFO")

# if args.debug:
#     print("gotta debug")
#     logger.remove()
#     logger.add(sys.stderr, level="DEBUG")
#     logger.debug(f"{args = }")


class Reducer(Protocol):
    def reduce(self, xs: list[float]) -> float:
        """Return a single value from a list of values."""


class Mean(Reducer):
    def reduce(self, xs: list[float]) -> float:
        return 0 if len(xs) == 0 else sum(xs) / len(xs)


class Median(Reducer):
    def reduce(self, xs: list[float]) -> float:
        xs_sorted = sorted(xs)
        n = len(xs_sorted)
        if n == 0:
            return 0
        elif n % 2 == 0:
            return (xs_sorted[n // 2 - 1] + xs_sorted[n // 2]) / 2
        else:
            return xs_sorted[n // 2]


@dataclass
class LightLevelMeasurement:
    light_level: float
    timestamp: int

    def __post_init__(self) -> None:
        if self.light_level < 0:
            raise ValueError(
                f"self.light_level must be non-negative, but is {self.light_level}"
            )
        if self.timestamp < 0:
            raise ValueError(
                f"self.timestamp must be non-negative, but is {self.timestamp}"
            )


app = Flask(__name__)


def get_db() -> sqlite3.Connection:
    global config
    if "db" not in g:
        g.db = sqlite3.connect(config["server"]["streetlamps"]["db-path"])
    return g.db


@app.teardown_appcontext
def close_connection(exception) -> None:
    db = g.pop("db", None)
    if db is not None:
        db.close()


@app.route("/streetlamp/<int:streetlamp_id>/lightlevels", methods=["GET"])
def get_timeseries(streetlamp_id: int) -> Response:
    logger.debug(f"{request.url = }")
    if args.debug:
        for k, v in request.args.items():
            logger.error(f"{k} = {v}")

    match request.args.get("reducer", default="mean"):
        case "mean":
            reducer = Mean()
        case "median":
            reducer = Median()
        case _:
            return jsonify({"error": "unknown reducer"})

    match request.args.get("per", default="hour"):
        case "hour":
            per = timedelta(hours=1)
        case "day":
            per = timedelta(days=1)
        case "week":
            per = timedelta(weeks=1)
        case _:
            return jsonify({"error": "unknown per"})

    if "start" not in request.args:
        return jsonify({"error": "start is not specified"})
    start = int(request.args["start"])
    # start = datetime.fromtimestamp(int(request.args['start']))

    if "end" not in request.args:
        return jsonify({"error": "end is not specified"})
    end = int(request.args["end"])
    # end = datetime.fromtimestamp(int(request.args['end']))

    if start > end:
        return jsonify({"error": "start is greater than end"})

    logger.debug(f"{start = } {end = } {per = } {reducer = }")

    db = get_db()
    cursor = db.cursor()
    query = """
    SELECT light_level, timestamp FROM measurements
    WHERE streetlamp_id = ? and timestamp BETWEEN ? AND ?
    ORDER BY timestamp ASC;
"""

    cursor.execute(query, (streetlamp_id, start, end))
    # cursor.execute("select * from streetlamps;")
    rows = cursor.fetchall()
    measurements = [LightLevelMeasurement(*row) for row in rows]
    # logger.debug(f"{measurements = }")

    num_bins: int = int((end - start) / per.seconds)
    logger.debug(f"{num_bins = }")

    reduced_light_levels: list[float] = []
    start_idx: int = 0
    end_idx: int = 0
    for i in range(num_bins):
        # Find end
        while (
            end_idx < len(measurements)
            and measurements[end_idx].timestamp < start + (i + 1) * per.seconds
        ):
            end_idx += 1

        reduction: float = reducer.reduce(
            [m.light_level for m in measurements[start_idx:end_idx]]
        )
        reduced_light_levels.append(reduction)
        start_idx = end_idx

    assert (
        len(reduced_light_levels) == num_bins
    ), f"{len(reduced_light_levels) = } != {num_bins = }"

    # logger.debug(f"{reduced_light_levels = }")
    return jsonify(reduced_light_levels)


if __name__ == "__main__":
    app.run(
        host="localhost", port=config["server"]["streetlamps"]["port"], debug=args.debug
    )
