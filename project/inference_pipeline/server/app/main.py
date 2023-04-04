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

## socket
# create a socket object
serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# get local machine name
host = socket.gethostbyname("localhost")
# set the port number to listen on
port1 = 5000

# bind the socket to a specific address and port
serversocket.bind(("", port1))

########################### NCM ############################
# define your expected format string and expected size
expected_format = "32siffffffffffiiffiiiiiiiiiiiiiiiiiii"
expected_size = struct.calcsize(expected_format)


def receive_data_ncm(client_socket_ncm):
    while True:
        data = client_socket_ncm.recv(1024)
        if not data:
            break

        if len(data) != expected_size:
            print(f"Error: Expected {expected_size} bytes but got {len(data)} bytes")
        else:
            session_info = struct.unpack(expected_format, data)
            session_dict = {
                # From session info struct
                "session": session_info[0],
                "count": session_info[1],
                "TX_data": session_info[2],
                "TX_ack": session_info[3],
                "RX_data": session_info[4],
                "RX_ack": session_info[5],
                "RX_EXCESS_DATA": session_info[6],
                "RX_LATE_DATA": session_info[7],
                "RX_LATE_ACK": session_info[8],
                "TX_REDUNDANT": session_info[9],
                "redundancy": session_info[10],
                "uplink": session_info[11],
                "p": session_info[12],
                "q": session_info[13],
                "downlink": session_info[14],
                "qdelay": session_info[15],
                # From radiotap header
                "rate": session_info[16],
                "channelFrequency": session_info[17],
                "channelFlags": session_info[18],
                "signal": session_info[19],
                "noise": session_info[20],
                "lockQuality": session_info[21],
                "TX_attenuation": session_info[22],
                "TX_attenuation_dB": session_info[23],
                "TX_power": session_info[24],
                "antenna": session_info[25],
                "signal_dB": session_info[26],
                "noise_dB": session_info[27],
                "RTS_retries": session_info[28],
                "DATA_retries": session_info[29],
                "MCS_known": session_info[30],
                "MCS_flags": session_info[31],
                "MCS_MCS": session_info[32],
                # Remote address + Master or Slave
                "remoteAddress": session_info[33],
                "masterOrSlave": session_info[34],  # // -1 neither, 0 slave, 1 master
            }

            # Print the decoded data
            print(session_dict)


# Create a socket object
server_socket_ncm = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# Bind the socket to a specific IP address and port
server_address_ncm = ("127.0.0.1", 10123)
server_socket_ncm.bind(server_address_ncm)

# Listen for incoming connections
server_socket_ncm.listen(1)

# Accept an incoming connection
client_socket_ncm, client_address_ncm = server_socket_ncm.accept()
print("Connected by", client_address_ncm)

# Create a new thread to receive data on the socket
receive_thread = threading.Thread(target=receive_data_ncm, args=(client_socket_ncm,))
receive_thread.start()
########################### NCM ############################


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
    serversocket2.listen(1)
    # wait for a client to connect
    clientsocket, addr = serversocket.accept()
    clientsocket2, addr = serversocket2.accept()

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
