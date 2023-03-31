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
import threading

app = FastAPI()

templates = Jinja2Templates(directory="/code/app/templates")

random.seed()  # Initialize the random number generator

## -------------------------------- Inference -> Dashboard --------------------------------
# create a socket object
serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# get local machine name
host = socket.gethostbyname("localhost")
# set the port number to listen on
port1 = 5000

serversocket.bind(("", port1))

# start listening for incoming connections
serversocket.listen(1)

# wait for a client to connect
clientsocket, addr = serversocket.accept()

# ########################### NCM ############################
# def receive_data_ncm(client_socket_ncm):
#     while True:
#         data = client_socket_ncm.recv(1024)
#         if not data:
#             break
#         print(f"NCM received data: {data.decode()}")


# # Create a socket object
# server_socket_ncm = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# # Bind the socket to a specific IP address and port
# server_address_ncm = ("127.0.0.1", 10123)
# server_socket_ncm.bind(server_address_ncm)

# # Listen for incoming connections
# server_socket_ncm.listen(1)

# # Accept an incoming connection
# client_socket_ncm, client_address_ncm = server_socket_ncm.accept()
# print("Connected by", client_address_ncm)

# # Create a new thread to receive data on the socket
# receive_thread = threading.Thread(target=receive_data_ncm, args=(client_socket_ncm,))
# receive_thread.start()
# ########################### NCM ############################


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

    while True:
        # receive data from the sender
        num_bytes = clientsocket.recv(4)

        # Unpack the byte string into a float
        num = struct.unpack("f", num_bytes)[0]

        # Print the received float
        print("Received:", num)

        json_data = json.dumps(
            {
                "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "value": num,
            }
        )
        yield f"data:{json_data}\n\n"
        await asyncio.sleep(1)
