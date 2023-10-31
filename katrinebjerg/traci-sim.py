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


# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass


if 'SUMO_HOME' in os.environ:
    sys.path.append(os.path.join(os.environ['SUMO_HOME'], 'tools'))

import traci
print(f"{traci.__file__ = }")
print(f"{traci.__path__ = }")
# import libsumo as traci

import traci.constants as tc
print(f"{tc.__file__    = }")

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

print(f"{traci = }")

SUMO_BIN: str | None = shutil.which("sumo")
assert SUMO_BIN is not None, f"sumo was not found in PATH: {sys.path}"

SUMO_GUI_BIN: str | None = shutil.which("sumo-gui")
assert SUMO_GUI_BIN is not None, f"sumo-gui was not found in PATH: {sys.path}"

print(f"{SUMO_BIN = }")
print(f"{SUMO_GUI_BIN = }")

print(traci.__dict__.keys())

scriptname: str = os.path.basename(__file__).removesuffix(".py")
argparser = argparse.ArgumentParser(prog=scriptname)

argparser.add_argument("sumocfg", help="Path to SUMO configuration file")

args = argparser.parse_args()

print(f"{args = }")

sumocfg = Path(args.sumocfg)
assert sumocfg.exists(), f"SUMO configuration file not found: {sumocfg}"

sumo_cmd: list[str] = [SUMO_GUI_BIN, "-c", str(sumocfg)]
print(f"{sumo_cmd = }")

traci.start(sumo_cmd, label="sim0")

@dataclass(frozen=True)
class SimulationParameters:
    max_steps: int

    def __post_init__(self):
        assert 0 < self.max_steps, f"{self.max_steps = } < 0"


sim_params = SimulationParameters(max_steps=1000)

junctions = traci.junction.getIDList()


print(f"{junctions = }")

print(f"{traci.junction.__dict__ = }")

dt: float = traci.simulation.getDeltaT()
print(f"{dt = }")
# step: int = 0
# while step < sim_params.max_steps:
#     traci.simulationStep()
#     # if traci.inductionloop.getLastStepVehicleNumber("0") > 0:
#     #     traci.trafficlight.setRedYellowGreenState("0", "GrGr")
#     step += 1
#     time.sleep(0.1)

# traci.close()


# x, y = traci.vehicle.getPosition(vehID)
