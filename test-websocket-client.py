import argparse
import os
import sys
import asyncio

import cbor2
import websockets

async def test_websocket(uri):
    # Connect to the WebSocket server
    async with websockets.connect(uri) as websocket:
        while True:  # Loop to send messages periodically or based on some condition
            try:
                # Send a CBOR encoded message
                await websocket.send(cbor2.dumps("Hello"))
                # Wait for a response
                message = await websocket.recv()
                # Expects that message is encoded in CBOR
                print(f"Received (not decoded): {message}")
                message_decoded = cbor2.loads(message)
                # print(f"Received: {message_decoded}")
                # Sleep to simulate periodic sending
                await asyncio.sleep(0.25)  # seconds
            except websockets.ConnectionClosed:
                print("Connection to server closed.")
                break  # Exit the loop if the connection is closed
            except KeyboardInterrupt:
                print("Keyboard interrupt")
                break
            # You can add more specific exception handling as needed

def main(argc: int, argv: list[str]) -> int:
    script_name = os.path.basename(os.path.realpath(__file__)).removesuffix(".py")
    argv_parser = argparse.ArgumentParser(prog=script_name, description="Test websocket client")
    argv_parser.add_argument("-p", "--port", type=int, required=True, help=f"Port to connect to 0 < port < {2**16 - 1}")

    args = argv_parser.parse_args(argv[1:])
    assert 0 < args.port < 2**16 - 1, f"Port must be 0 < port < {2**16 - 1}"
    route: str = "cars"
    uri: str = f"ws://localhost:{args.port}/{route}"

    asyncio.run(test_websocket(uri))
    return 0

if __name__ == "__main__":
    sys.exit(main(len(sys.argv), sys.argv))
