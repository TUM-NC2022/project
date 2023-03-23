import socket
import struct

HOST = "localhost"
PORT = 10123
# define your expected format string and expected size
expected_format = "32siffffffffffiiffiiiiiiiiiiiiiiiiiii"
expected_size = struct.calcsize(expected_format)

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind(("localhost", PORT))
server_socket.listen(1)
print(f"Server listening on port {PORT}")

while True:
    connection, address = server_socket.accept()
    print(f"Connected by: {address}")

    while True:
        data = connection.recv(1024)
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

    print("Connection closed")
    connection.close()
