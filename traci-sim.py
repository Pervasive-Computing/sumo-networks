# %%

import os
import sys
import shutil
import argparse
from pprint import pprint
import json
from pathlib import Path
from dataclasses import dataclass
import time
import math
from datetime import datetime
import logging
import random

# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass

import xmltodict

def xml_to_dict(file_path: Path | str) -> dict:
    with open(file_path, 'r') as file:
        xml_content = file.read()
        dict_content = xmltodict.parse(xml_content, attr_prefix='')
        return dict_content


if 'SUMO_HOME' in os.environ:
    sys.path.append(os.path.join(os.environ['SUMO_HOME'], 'tools'))
else:
    print("Make sure the SUMO_HOME environment variable is set.", file=sys.stderr)
    sys.exit(1)

import traci
# import libsumo as traci
import traci.constants as tc


## ANSI escape sequences for colors
class LogColors:
    RESET = "\033[0m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"

class CustomFormatter(logging.Formatter):
    """Logging Formatter to add colors and time stamp"""

    # format = "%(asctime)s - [%(levelname)s] - %(message)s"

    FORMATS = {
        logging.DEBUG: LogColors.BLUE,
        logging.INFO: LogColors.GREEN,
        logging.WARNING: LogColors.YELLOW,
        logging.ERROR: LogColors.RED,
        logging.CRITICAL: LogColors.RED,
    }

    def format(self, record):
        color: str = self.FORMATS.get(record.levelno)
        log_fmt = f"%(asctime)s - {color}[%(levelname)s]{LogColors.RESET} - %(message)s"
        formatter = logging.Formatter(log_fmt, "%Y-%m-%d %H:%M:%S")
        return formatter.format(record)

class LevelNameFilter(logging.Filter):
    def filter(self, record):
        level_to_name = {
            'WARNING': 'warn',
            'ERROR': 'error',
            'INFO': 'info'
        }
        record.levelname = level_to_name.get(record.levelname, record.levelname.lower())
        return True

# Create a logger
logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# Create a console handler and set format with a timestamp
handler = logging.StreamHandler()
handler.setFormatter(CustomFormatter())

# Add filter to handler to modify level names
handler.addFilter(LevelNameFilter())

# Add the handler to the logger
logger.addHandler(handler)

SCRIPTNAME: str = os.path.basename(__file__).removesuffix(".py")
SCRIPTDIR: str = os.path.dirname(__file__)
argparser = argparse.ArgumentParser(prog=SCRIPTNAME)

argparser.add_argument("sumocfg", help="Path to SUMO configuration file")
argparser.add_argument("--log", action="store_true",help="Log to log.txt")
argparser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
argparser.add_argument("--gui", action="store_true", default=False, help="Use `sumo-gui` instead of `sumo` to run the simulation")

args = argparser.parse_args()

if args.verbose:
    def show(*args, **kwargs) -> None:
        return print(*args, **kwargs)
else:
    def show(*args, **kwargs) -> None:
        pass

show(f"{traci.__file__ = }")
show(f"{traci.__path__ = }")
show(f"{traci.isLibtraci() = }")
show(f"{traci.isLibsumo()  = }")

show(f"{tc.__file__    = }")

# Print all constants in traci.constants
tc_items = sorted(list(tc.__dict__.items()), key=lambda it: it[0])
tc_keys: list[str] = [key for key, _ in tc_items]
tc_values: list[int] = [value for _, value in tc_items]
tc_keys_max_length = max([len(k) for k in tc_keys])
tc_values = list(tc.__dict__.values())
for i in range(len(tc_keys)):
    k = tc_keys[i]
    v = tc_values[i]
    print(f"{k:<{tc_keys_max_length}} = {v}")

# sys.exit(0)

show(f"{traci = }")
show(traci.__dict__.keys())

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

logger.info(f"{running_in_wsl() = }")

# Binaries
SUMO_BIN: str | None = shutil.which(f"sumo{postfix}")
assert SUMO_BIN is not None, f"sumo was not found in PATH: {sys.path}"

SUMO_GUI_BIN: str | None = shutil.which(f"sumo-gui{postfix}")
assert SUMO_GUI_BIN is not None, f"sumo-gui was not found in PATH: {sys.path}"

print(f"{SUMO_BIN = }")
print(f"{SUMO_GUI_BIN = }")

show(f"{args = }")

sumocfg_path = Path(args.sumocfg)
assert sumocfg_path.exists(), f"SUMO configuration file not found: {sumocfg_path}"

sumocfg = xml_to_dict(sumocfg_path)["configuration"]

print(f"{sumocfg = }")

# print(f"{sumocfg['input'] = }")

# pprint(sumocfg["input"], indent=4)
# print(json.dumps(sumocfg["input"], indent=4))

route_files: str = sumocfg["input"]["route-files"]['value']
net_file: str = sumocfg["input"]["net-file"]['value']
additional_files: str | None = None
if "additional-files" in sumocfg["input"]:
    additional_files = sumocfg["input"]["additional-files"]['value']

print(f"{route_files = }")
print(f"{net_file = }")
print(f"{additional_files = }")

# Assume that the input files in the SUMO configuration are relative to the
# directory of the configuration file, so we need to change the working
# directory to the directory of the configuration file.
os.chdir(sumocfg_path.parent)
sumocfg_path = sumocfg_path.name

print(f"{os.getcwd() = }")

sumo_cmd: list[str] = [SUMO_GUI_BIN if args.gui else SUMO_BIN, "-c", str(sumocfg_path)]
if args.log:
    timestr = datetime.now().strftime("%Y%m%d-%H%M%S")
    logfile = f"log-{timestr}.log"
    assert Path(logfile).exists() is False, f"Log file already exists: {logfile}"
    sumo_cmd += ["--log", logfile]
print(f"{sumo_cmd = }")


@dataclass(frozen=True)
class SimulationParameters:
    max_steps: int

    def __post_init__(self):
        assert 0 < self.max_steps, f"{self.max_steps = } < 0"


sim_params = SimulationParameters(max_steps=100_000)

print(f"{sim_params = }")

logger.info("Starting SUMO")
traci.start(sumo_cmd, label="sim0")


junctions = traci.junction.getIDList()
print(f"{junctions = }")


# show(f"{traci.junction.__dict__ = }")

dt: float = traci.simulation.getDeltaT()
print(f"{dt = }")
step: int = 0

TRACI_VEHICLE_CONSTANTS: list[int] = [
    tc.VAR_POSITION,
    # tc.VAR_SIGNALS,
    # tc.VAR_VEHICLECLASS
    tc.VAR_SPEED,
    # tc.VAR_ACCELERATION,
    tc.VAR_ACCEL,
]

@dataclass(frozen=True)
class Vehicle:
    speed: float
    x: float
    y: float
    angle: float

    def __post_init__(self) -> None:
        pass


class ExampleListener(traci.StepListener):
    def step(self, t: float) -> bool:
        print(f"{self.__class__ = } called with parameter {t}")
        # indicate that the step listener should stay active in the next step
        return True

traci.addStepListener(ExampleListener())

# traci.vehicle.subscribe(vehID, (tc.VAR_ROAD_ID, tc.VAR_LANEPOSITION))
# print(traci.vehicle.getSubscriptionResults(vehID))

# Used to keep track of the vehicles in the simulation
# so we do not subscribe to them more than once
vehicles: set[int] = set()

t_begin: float = time.time()
while step < sim_params.max_steps:
    t_start: float = time.time()
    traci.simulationStep()
    t_end: float = time.time()
    t_delta: float = t_end - t_start
    # print(f"{t_delta = }")
    step += 1

    # use ^ for symmetric difference
    vehicles_at_this_step: set[int] = set(traci.vehicle.getIDList())
    new_vehicles: set[int] = vehicles_at_this_step ^ vehicles
    if new_vehicles != {}:
        for veh_id in new_vehicles:
            traci.vehicle.subscribe(veh_id, TRACI_VEHICLE_CONSTANTS)
            logger.info(f"Subscribed to {veh_id = }")

        vehicles |= new_vehicles

    # sub_results = traci.vehicle.getSubscriptionResults(list(vehicles))
    sub_results = [traci.vehicle.getSubscriptionResults(veh_id) for veh_id in vehicles]
    print(f"{sub_results = }")


    # if step % 100 == 0:
    #     print(f"{step = }")
    #     vehicles = traci.vehicle.getIDList()
    #     print(f"{vehicles = }")
    #     print(f"{type(vehicles) = }")
    #     if len(vehicles) == 0:
    #         continue
    #     random_vec_id: int = random.choice(vehicles)
    #     traci.vehicle.highlight(random_vec_id, color=(255, 0, 0), size=2)
    #     for vec_id in vehicles:
    #         speed: float = traci.vehicle.getSpeed(vec_id)
    #         x, y = traci.vehicle.getPosition(vec_id)
    #         lon, lat = traci.simulation.convertGeo(x, y)
    #         x2, y2 = traci.simulation.convertGeo(lon, lat, fromGeo=True)

    #         print(f"{vec_id = } {x = } {y = } {lon = } {lat = } {x2 = } {y2 = }")

    #         traci.vehicle.subscribe(vec_id, TRACI_VEHICLE_CONSTANTS)

            # sub_results = traci.vehicle.getSubscriptionResults()
            # print(f"{sub_results = }")


        # print(f"{vehicles = }")
    # time.sleep(0.1)


# x, y = traci.vehicle.getPosition(vehID)
show(f"{traci.lane.__dict__ = }")
show(f"{traci.vehicle.__dict__ = }")
show(f"{traci.trafficlight.__dict__ = }")

traci.close() # Stop SUMO simulation
