import socket
import struct
import time
import threading
import random

## -------------------------------- NCM mock -> Inference ---------------------------------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
#host = socket.gethostbyname("host.docker.internal")  # Access the host ip #TODO
host = socket.gethostbyname('localhost')  # Access the host ip # TODO to be removed
print(host)
PORT_INF_DASH = 10123
s.connect((host, PORT_INF_DASH))
## ----------------------------------------------------------------------------------------

expected_format = "32siffffffffffiiffiiiiiiiiiiiiiiiiiii"

# send mock data to inference and receive lqe
def send_mock_data():
    while True:
        # mock data
        mock_data1 = {'session': b'e0:7:34:c5:13:56:e0:7:34:c5:13:\x00', 'count': 8, 'TX_data': 63.375, 'TX_ack': 90.25, 'RX_data': 97.625, 'RX_ack': 20.5, 'RX_EXCESS_DATA': 34.25, 'RX_LATE_DATA': 0.75, 'RX_LATE_ACK': 37.5, 'TX_REDUNDANT': 37.5, 'redundancy': 1.0276679992675781, 'uplink': 0.9730769395828247, 'p': 253, 'q': 7, 'downlink': 0.9467213153839111, 'qdelay': 1.1210821866989136, 'rate': 0, 'channelFrequency': 2412, 'channelFlags': 1152, 'signal': -40, 'noise': 0, 'lockQuality': 0, 'TX_attenuation': 0, 'TX_attenuation_dB': 41400, 'TX_power': -48, 'antenna': 1, 'signal_dB': 254, 'noise_dB': 172, 'RTS_retries': 0, 'DATA_retries': 0, 'MCS_known': 39, 'MCS_flags': 1, 'MCS_MCS': 0, 'remoteAddress': 62, 'masterOrSlave': 1}
        mock_data2 = {'session': b'e0:7:34:c5:13:56:e0:7:34:c5:13:\x00', 'count': 8, 'TX_data': 63.375, 'TX_ack': 90.25, 'RX_data': 97.625, 'RX_ack': 20.5, 'RX_EXCESS_DATA': 34.25, 'RX_LATE_DATA': 0.75, 'RX_LATE_ACK': 37.5, 'TX_REDUNDANT': 37.5, 'redundancy': 1.0276679992675781, 'uplink': 0.9730769395828247, 'p': 253, 'q': 7, 'downlink': 0.9467213153839111, 'qdelay': 1.1210821866989136, 'rate': 0, 'channelFrequency': 2412, 'channelFlags': 1152, 'signal': -70, 'noise': 0, 'lockQuality': 0, 'TX_attenuation': 0, 'TX_attenuation_dB': 41400, 'TX_power': -48, 'antenna': 1, 'signal_dB': 254, 'noise_dB': 172, 'RTS_retries': 0, 'DATA_retries': 0, 'MCS_known': 39, 'MCS_flags': 1, 'MCS_MCS': 0, 'remoteAddress': 62, 'masterOrSlave': 1}
        mock_data3 = {'session': b'e0:7:34:c5:13:56:e0:7:34:c5:13:\x00', 'count': 8, 'TX_data': 63.375, 'TX_ack': 90.25, 'RX_data': 97.625, 'RX_ack': 20.5, 'RX_EXCESS_DATA': 34.25, 'RX_LATE_DATA': 0.75, 'RX_LATE_ACK': 37.5, 'TX_REDUNDANT': 37.5, 'redundancy': 1.0276679992675781, 'uplink': 0.9730769395828247, 'p': 253, 'q': 7, 'downlink': 0.9467213153839111, 'qdelay': 1.1210821866989136, 'rate': 0, 'channelFrequency': 2412, 'channelFlags': 1152, 'signal': -35, 'noise': 0, 'lockQuality': 0, 'TX_attenuation': 0, 'TX_attenuation_dB': 41400, 'TX_power': -48, 'antenna': 1, 'signal_dB': 254, 'noise_dB': 172, 'RTS_retries': 0, 'DATA_retries': 0, 'MCS_known': 39, 'MCS_flags': 1, 'MCS_MCS': 0, 'remoteAddress': 62, 'masterOrSlave': 1}
        mock_data4 = {'session': b'e0:7:34:c5:13:56:e0:7:34:c5:13:\x00', 'count': 8, 'TX_data': 63.375, 'TX_ack': 90.25, 'RX_data': 97.625, 'RX_ack': 20.5, 'RX_EXCESS_DATA': 34.25, 'RX_LATE_DATA': 0.75, 'RX_LATE_ACK': 37.5, 'TX_REDUNDANT': 37.5, 'redundancy': 1.0276679992675781, 'uplink': 0.9730769395828247, 'p': 253, 'q': 7, 'downlink': 0.9467213153839111, 'qdelay': 1.1210821866989136, 'rate': 0, 'channelFrequency': 2412, 'channelFlags': 1152, 'signal': -85, 'noise': 0, 'lockQuality': 0, 'TX_attenuation': 0, 'TX_attenuation_dB': 41400, 'TX_power': -48, 'antenna': 1, 'signal_dB': 254, 'noise_dB': 172, 'RTS_retries': 0, 'DATA_retries': 0, 'MCS_known': 39, 'MCS_flags': 1, 'MCS_MCS': 0, 'remoteAddress': 62, 'masterOrSlave': 1}
        mock_data5 = {'session': b'e0:7:34:c5:13:56:e0:7:34:c5:13:\x00', 'count': 8, 'TX_data': 63.375, 'TX_ack': 90.25, 'RX_data': 97.625, 'RX_ack': 20.5, 'RX_EXCESS_DATA': 34.25, 'RX_LATE_DATA': 0.75, 'RX_LATE_ACK': 37.5, 'TX_REDUNDANT': 37.5, 'redundancy': 1.0276679992675781, 'uplink': 0.9730769395828247, 'p': 253, 'q': 7, 'downlink': 0.9467213153839111, 'qdelay': 1.1210821866989136, 'rate': 0, 'channelFrequency': 2412, 'channelFlags': 1152, 'signal': -95, 'noise': 0, 'lockQuality': 0, 'TX_attenuation': 0, 'TX_attenuation_dB': 41400, 'TX_power': -48, 'antenna': 1, 'signal_dB': 254, 'noise_dB': 172, 'RTS_retries': 0, 'DATA_retries': 0, 'MCS_known': 39, 'MCS_flags': 1, 'MCS_MCS': 0, 'remoteAddress': 62, 'masterOrSlave': 1}
        mock_data = [mock_data1, mock_data2, mock_data3, mock_data4, mock_data5]
        #print mock_data dimensions
        print(len(mock_data))
        # pack the dictionary into a byte string with both the key and value
        rand = random.randint(0,4)
        mock_data_rand = mock_data[rand]
        packed_data = struct.pack(expected_format, *mock_data_rand.values())
        s.sendall(packed_data)

        lqe = s.recv(1024)
        print(lqe)

        time.sleep(1)

# run send_mock_data() in seperate thread
send = threading.Thread(target=send_mock_data)
send.start()

