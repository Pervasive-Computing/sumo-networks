import json
import os

import argparse

# Check if rich is installed, and only import it if it is.
try:
    from rich import pretty, print

    pretty.install()
except ImportError or ModuleNotFoundError:
    pass

import traci.constants as tc


def is_dunder(name: str) -> bool:
    return name.startswith("__") and name.endswith("__")


def get_traci_constants() -> list[tuple[str, int]]:
    items = tc.__dict__.items()
    items_without_dunder_props = [item for item in items if not is_dunder(item[0])]
    return sorted(items_without_dunder_props, key=lambda it: it[0])


if __name__ == "__main__":
    description: str = "Print all constants in traci.constants"
    scriptname: str = os.path.basename(__file__).removesuffix(".py")
    parser = argparse.ArgumentParser(prog=scriptname, description=description)
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    args = parser.parse_args()

    constants = get_traci_constants()

    if args.json:
        constants_as_dict: dict[str, int] = {}
        for name, value in constants:
            constants_as_dict[name] = value
        print(json.dumps(constants_as_dict, indent=4))
    else:
        tc_keys: list[str] = [key for key, _ in constants]
        tc_values: list[int] = [value for _, value in constants]
        tc_keys_max_length: int = max([len(k) for k in tc_keys])
        tc_values = list(tc.__dict__.values())
        for i in range(len(tc_keys)):
            k = tc_keys[i]
            v = tc_values[i]
            print(f"{k:<{tc_keys_max_length}} = {v}")
