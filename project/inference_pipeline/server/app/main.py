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
import logging
import multiprocessing
import asyncio


app = FastAPI()

templates = Jinja2Templates(directory="/code/app/templates")

# configure the logger
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
    handlers=[logging.FileHandler("app.log"), logging.StreamHandler()],
)

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
def start_socket_connection_ncm(host, port):
    server_socket_ncm = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket_ncm.bind((host, port))
    server_socket_ncm.listen(5)
    logging.info(f"Started raw socket server at {host}:{port}")

    while True:
        conn, addr = server_socket_ncm.accept()
        logging.info(f"Connected by {addr}")
        # client_thread = threading.Thread(target=receive_data_ncm, args=(conn,))
        client_process = multiprocessing.Process(target=receive_data_ncm, args=(conn,))
        # client_thread.start()
        client_process.start()
        conn.close()


def receive_data_ncm(client_socket_ncm):
    # define your expected format string and expected size
    expected_format = "32siffffffffffiiffiiiiiiiiiiiiiiiiiii"
    expected_size = struct.calcsize(expected_format)

    logging.info(f"Info: Wait to receive data from client")

    while True:
        data = client_socket_ncm.recv(1024)
        if not data:
            logging.info(f"Error: Data not properly received")
            break

        if len(data) != expected_size:
            logging.info(
                f"Error: Expected {expected_size} bytes but got {len(data)} bytes"
            )
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

            logging.info(f"Received data: {session_dict}")

    client_socket_ncm.close()
    logging.info(f"Closed socket")


async def receive_data_ncm_new(reader, writer):
    logging.info(f"Connected by {writer.get_extra_info('peername')}")
    while True:
        data = await reader.read(1024)
        if not data:
            break
        message = data.decode()
        logging.info(f"Received: {message}")


# async def start_socket_connection_ncm(host, port):
#     server = await asyncio.start_server(receive_data_ncm_new, host, port)
#     logging.info(f"Started raw socket server at {host}:{port}")

#     async with server:
#         await server.serve_forever()


@app.on_event("startup")
async def on_startup():
    host, port = "0.0.0.0", 10123
    server_thread = threading.Thread(
        target=start_socket_connection_ncm, args=(host, port)
    )
    server_thread.start()
    # server_process = multiprocessing.Process(target=start_socket_connection_ncm, args=(host, port))
    # server_process.start()

    # asyncio.create_task(start_socket_connection_ncm(host, port))


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
    # wait for a client to connect
    clientsocket, addr = serversocket.accept()

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
