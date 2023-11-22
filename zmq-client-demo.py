import os
import sys
import time
import argparse

import zmq
import cbor2
try:
    from rich import print, pretty
    pretty.install()
except ImportError or ModuleNotFoundError:
    pass

def clear_screen() -> None:
    # printf "\033c"
    print("\033c", file=sys.stdout, flush=False)

def main(argc: int, argv: list[str]) -> int:
    argv_parser = argparse.ArgumentParser(prog=os.path.basename(__file__).removesuffix(".py"),
                                         description="ZeroMQ client demo")
    argv_parser.add_argument("-p", "--port", type=int, default=5555, help="Port number")

    args = argv_parser.parse_args(argv[1:])

    context = zmq.Context()

    print(f"Connecting to server on port {args.port}...")
    subscriber = context.socket(zmq.SUB)
    subscriber.connect(f"tcp://localhost:{args.port}")
    subscriber.setsockopt(zmq.SUBSCRIBE, b"")
    print("Connected!")

    n_messages_received: int = 0
    try:
        while True:
            message = subscriber.recv()
            n_messages_received += 1
            data = cbor2.loads(message)
            clear_screen()
            print(f"Received message #{n_messages_received}: {data}")
            # time.sleep(0.1)
    except KeyboardInterrupt:
        print("Interrupted!")
    finally:
        subscriber.close()
        context.term()

    return 0


if __name__ == "__main__":
    sys.exit(main(len(sys.argv), sys.argv))
