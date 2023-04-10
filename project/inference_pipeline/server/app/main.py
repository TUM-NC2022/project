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
import logging
import asyncio
import numpy as np
import joblib
import time


app = FastAPI()
SEED = 0xDEADBEEF
np.random.seed(SEED)
np.set_printoptions(suppress=True)

dtree = joblib.load("app/dtree.joblib")
expected_format = "32siffffffffffiiffiiiiiiiiiiiiiiiiiii"
expected_size = struct.calcsize(expected_format)

memory = joblib.Memory(location=".cache", verbose=0)

templates = Jinja2Templates(directory="/code/app/templates")

host = "0.0.0.0"
port = 10123
server_socket_ncm = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket_ncm.bind((host, port))
server_socket_ncm.listen(5)
connection , addr = server_socket_ncm.accept()
logging.info(f"Started raw socket server at {host}:{port}")

# configure the logger
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
    handlers=[logging.FileHandler("app.log"), logging.StreamHandler()],
)

random.seed()
labels = ["good", "interm.", "bad"]
rssi_window10 = []

def convert_rssi_to_value(rssi):
    print("RSSI: " + str(rssi))
    if rssi > -35:
        return 128  # Error
    if rssi < -95:
        return 128  # Error
    else:
        value = rssi + 95
        return value if value >= 0 else 0 

@app.get("/", response_class=HTMLResponse)
async def read_root(request: Request):
    return templates.TemplateResponse("index.html", {"request": request})


@app.get("/chart-data")
async def chart_data(request: Request) -> StreamingResponse:
    response = StreamingResponse(receive_data(request), media_type="text/event-stream")
    response.headers["Cache-Control"] = "no-cache"
    response.headers["X-Accel-Buffering"] = "no"
    return response

async def receive_data(request: Request) -> Iterator[str]:
    while True:
        print(time.strftime("%H:%M:%S", time.localtime()), flush=True)

        data = connection.recv(1024)
        if not data:
            print("no data")
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

            rssi_dbm = session_dict["signal"]

            # plot the dimensions and datatypes
            rssi = convert_rssi_to_value(rssi_dbm)

            # add/replace the last value in the window
            if len(rssi_window10) < 10:
                rssi_window10.append(rssi)
            else:
                rssi_window10.pop(0)
                rssi_window10.append(rssi)

            rssi_avg = sum(rssi_window10) / len(rssi_window10)

            print("converted rssi: " + str(rssi))
            print("rssi avg: " + str(rssi_avg))

            y_pred = dtree.predict([[rssi, rssi_avg]])
            print("predicted lqe: " + y_pred)

            if y_pred == ['good']:
                y_pred = 2
            elif y_pred == ['bad']:
                y_pred = 0
            else:
                y_pred = 1

            json_data = json.dumps(
            {
                "time": datetime.now().strftime("%H:%M:%S"),
                "lqe": y_pred,
                "rss": rssi_dbm,
                "rss_avg": rssi_avg - 95 # normalize to dBm
            }
            )
        
            # yield for dashboard
            yield f"data:{json_data}\n\n"
            print("-> yielded to dashboard")
            
            # send lqe back to NCM
            packed_data = struct.pack('<I', y_pred)
            connection.sendall(packed_data)
            print("-> sent back to NCM")
            print("--------------------")

        await asyncio.sleep(1)
