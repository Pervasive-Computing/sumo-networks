# %%

import os
import sys
import shutil
import argparse
from pathlib import Path
from dataclasses import dataclass
import time
import math
from datetime import datetime
import logging

# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass


if 'SUMO_HOME' in os.environ:
    sys.path.append(os.path.join(os.environ['SUMO_HOME'], 'tools'))
else:
    print("'Please declare the environment variable \"SUMO_HOME\"", file=sys.stderr)
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
argparser = argparse.ArgumentParser(prog=SCRIPTNAME)

argparser.add_argument("sumocfg", help="Path to SUMO configuration file")
argparser.add_argument("--log", action="store_true",help="Log to log.txt")
argparser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")

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
# for i in range(len(tc_keys)):
#     k = tc_keys[i]
#     v = tc_values[i]
#     print(f"{k:<{tc_keys_max_length}} = {v}")

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

# if running_in_wsl():
#     print("Running in Windows Subsystem for Linux")
#     postfix = ".exe"
# else:
#     print("Not running in WSL")

# Binaries
SUMO_BIN: str | None = shutil.which(f"sumo{postfix}")
assert SUMO_BIN is not None, f"sumo was not found in PATH: {sys.path}"

SUMO_GUI_BIN: str | None = shutil.which(f"sumo-gui{postfix}")
assert SUMO_GUI_BIN is not None, f"sumo-gui was not found in PATH: {sys.path}"

print(f"{SUMO_BIN = }")
print(f"{SUMO_GUI_BIN = }")

show(f"{args = }")

sumocfg = Path(args.sumocfg)
assert sumocfg.exists(), f"SUMO configuration file not found: {sumocfg}"

os.chdir(sumocfg.parent)
sumocfg = sumocfg.name

print(f"{os.getcwd() = }")

sumo_cmd: list[str] = [SUMO_GUI_BIN, "-c", str(sumocfg)]
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

# junctions = traci.junction.getIDList()
# print(f"{junctions = }")

# show(f"{traci.junction.__dict__ = }")

logger.info("Starting SUMO")
traci.start(sumo_cmd, label="sim0")
dt: float = traci.simulation.getDeltaT()
print(f"{dt = }")
step: int = 0
while step < sim_params.max_steps:
    traci.simulationStep()
    # if traci.inductionloop.getLastStepVehicleNumber("0") > 0:
    #     traci.trafficlight.setRedYellowGreenState("0", "GrGr")
    step += 1
    # time.sleep(0.1)


# x, y = traci.vehicle.getPosition(vehID)
show(f"{traci.lane.__dict__ = }")
show(f"{traci.vehicle.__dict__ = }")
show(f"{traci.trafficlight.__dict__ = }")

traci.close()