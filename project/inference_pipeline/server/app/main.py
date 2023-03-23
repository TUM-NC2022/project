from fastapi import FastAPI, Request
from fastapi.templating import Jinja2Templates
from fastapi.responses import HTMLResponse, StreamingResponse
import random
import asyncio
import random
import json
import random
from typing import Iterator
from datetime import datetime
import socket
import struct

app = FastAPI()

templates = Jinja2Templates(directory="/code/app/templates")

random.seed()  # Initialize the random number generator

## socket
# create a socket object
serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# get local machine name
host = socket.gethostbyname("localhost")
# set the port number to listen on
port = 5000
# bind the socket to a specific address and port
serversocket.bind(("", port))

@app.get("/", response_class=HTMLResponse)
async def read_root(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})

@app.get("/chart-data")
async def chart_data(request: Request) -> StreamingResponse:
    response = StreamingResponse(receive_data(request), media_type="text/event-stream")
    response.headers["Cache-Control"] = "no-cache"
    response.headers["X-Accel-Buffering"] = "no"
    return response

async def generate_random_data(request: Request) -> Iterator[str]:
    """
    Generates random value [good, bad, intermediate] and current timestamp.
    :return: String containing current timestamp (YYYY-mm-dd HH:MM:SS) and randomly generated data.
    """
    client_ip = request.client.host

    while True:
        lqe = random.choice([0, 0.5, 1])

        json_data = json.dumps(
            {
                "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "value": lqe,
            }
        )
        yield f"data:{json_data}\n\n"
        await asyncio.sleep(1)

async def receive_data(request: Request) -> Iterator[str]:
    # start listening for incoming connections
    serversocket.listen(1)
    # wait for a client to connect
    clientsocket, addr = serversocket.accept()

    while True:
        # receive data from the sender
        num_bytes = clientsocket.recv(4)

        # Unpack the byte string into a float
        num = struct.unpack('f', num_bytes)[0]

        # Print the received float
        print('Received:', num)

        json_data = json.dumps(
            {
                "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "value": num,
            }
        )
        yield f"data:{json_data}\n\n"
        await asyncio.sleep(1)

