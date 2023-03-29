from asyncio import sleep
import os
import time
from helper.custom_interpolation import CustomInterpolation
from helper.custom_merger import CustomMerger
from helper.prr import PRR
from helper.synthetic_features import SyntheticFeatures
from sklearn import neural_network, preprocessing, tree, model_selection
from imblearn import pipeline, over_sampling
from imblearn import metrics as imetrics
import numpy as np
from datasets.trace1_Rutgers.transform import get_traces as load_rutgers, dtypes
import joblib
from typing import List, Tuple
import pandas as pd
from sklearn.model_selection import train_test_split
import socket
import struct
import random

SEED = 0xDEADBEEF
np.random.seed(SEED)
np.set_printoptions(suppress=True)

memory = joblib.Memory(location=".cache", verbose=0)


def prr_to_label(prr: float) -> str:
    if prr >= 0.9:
        return "good"
    elif prr <= 0.1:
        return "bad"
    return "interm."


labels = ["good", "interm.", "bad"]

## ----------------------------------- NCM -> Inference -----------------------------------
HOST = "localhost"
PORT_NCM_INF = 10123
expected_format = "32siffffffffffiiffiiiiiiiiiiiiiiiiiii"
expected_size = struct.calcsize(expected_format)

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind(("localhost", PORT_NCM_INF))
server_socket.listen(1)
print(f"Inference listening on port {PORT_NCM_INF}")

## -------------------------------- Inference -> Dashboard --------------------------------
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host = socket.gethostbyname("host.docker.internal")  # Access the host ip
#host = socket.gethostbyname('server')  # Access the host ip # TODO to be removed
print(host)
PORT_INF_DASH = 5000
s.connect((host, PORT_INF_DASH))


## general inits
@memory.cache
def prepare_data():
    dataset = list(load_rutgers())
    print("Rutgers loaded ...")

    dataset = CustomInterpolation(
        source="rssi", strategy="constant", constant=0
    ).fit_transform(dataset)
    print("Interpolation applied ...")

    dataset = SyntheticFeatures(source="rssi", window_size=10).fit_transform(dataset)
    print("Synthetic features created ...")

    dataset = PRR(
        source="received", window_size=10, ahead=1, target="prr"
    ).fit_transform(dataset)
    print("PRR created ...")

    print("Apply discrete derivation (backward difference)")
    for i in range(len(dataset)):
        dataset[i]["drssi"] = dataset[i]["rssi"].diff()

    dataset = CustomMerger().fit_transform(dataset)
    print("Datasets merged ...")

    dataset["class"] = dataset["prr"].apply(prr_to_label)
    print("Classification applied ...")

    dataset = dataset.dropna()
    dataset = dataset.drop(["noise", "src", "dst", "received", "prr"], axis=1)
    print("Drop useless features, drop lines with NaN")

    dataset = poly_features(
        dataset, include=["rssi", "rssi_avg", "rssi_std"], degree=4, include_bias=True
    )
    print("Polynomials applied ...")

    # Special synthetic features
    dataset["rssi^-1"] = 1.0 / dataset["rssi"]
    dataset["rssi^-2"] = 1.0 / dataset["rssi^2"]
    dataset["rssi^-3"] = 1.0 / dataset["rssi^3"]
    dataset["rssi^-4"] = 1.0 / dataset["rssi^4"]

    dataset["rssi_avg^-1"] = 1.0 / dataset["rssi_avg"]
    dataset["rssi_avg^-2"] = 1.0 / dataset["rssi_avg^2"]
    dataset["rssi_avg^-3"] = 1.0 / dataset["rssi_avg^3"]
    dataset["rssi_avg^-4"] = 1.0 / dataset["rssi_avg^4"]

    dataset["rssi_std^-1"] = 1.0 / dataset["rssi_std"]
    dataset["rssi_std^-2"] = 1.0 / dataset["rssi_std^2"]
    dataset["rssi_std^-3"] = 1.0 / dataset["rssi_std^3"]
    dataset["rssi_std^-4"] = 1.0 / dataset["rssi_std^4"]

    return dataset


def poly_features(
    df: pd.DataFrame,
    include: List[str],
    degree: int,
    include_bias=False,
    *args,
    **kwargs
) -> pd.DataFrame:
    """The `PolynomialFeatures` from sklern drops/loses information about column names from pandas, which is not very convinient.
    This is a workaround for this behaviour to preserve names.
    """
    X, excluded = df.loc[:, include], df.drop(include, axis=1)
    poly = preprocessing.PolynomialFeatures(
        degree=degree, include_bias=include_bias, *args, **kwargs
    ).fit(X)

    # Next line converts back to pandas, while preserving column names
    X = pd.DataFrame(
        poly.transform(X), columns=poly.get_feature_names(X.columns), index=X.index
    )

    data = pd.concat(
        [X, excluded],
        axis=1,
    )
    data = data.reset_index(drop=True)

    # Transform column names. Ex. 'rssi rssi_avg' -> 'rssi*rssi_avg'
    data = data.rename(lambda cname: cname.replace(" ", "*"), axis="columns")

    return data

# decision tree init
#features = ["rssi", "rssi_std", "rssi_avg"]
features = ["rssi"]
dtree = pipeline.Pipeline(
    [
        ("scaler", preprocessing.StandardScaler()),
        ("resample", over_sampling.RandomOverSampler(random_state=SEED)),
        ("dtree", tree.DecisionTreeClassifier(max_depth=4, random_state=SEED)),
    ]
)

# train decision tree
df = prepare_data()

# This is used for cross-validation. Typically used when observing performance of algorithm
cv = model_selection.StratifiedKFold(n_splits=3, shuffle=True, random_state=SEED)

dataset = df.replace([np.inf, -np.inf], np.nan).dropna(subset=features)

X, y = df.drop(["class"], axis=1), df["class"].ravel()
print(X)
print("-----------------")
print(X[features].values)
print("-----------------")
# Split the dataset into training and testing sets
X_train, X_test, y_train, y_test = train_test_split(
    X[features].values, y, random_state=10
)
dtree.fit(X_train, y_train)
print(X_test)
y_pred = dtree.predict(X_test)
print("-----------------")
print(y_pred)
# y_pred = model_selection.cross_val_predict(dtree, X[features].values, y, cv=cv, n_jobs=-1)
# print(imetrics.classification_report_imbalanced(y, y_pred, labels=labels))

def convert_rssi_to_value(rssi):
    if rssi < -127:
        return 128  # Error
    else:
        value = int((127/(-30)) * rssi + 127)
        return value if value >= 0 else 0
    
## ---------------------------------- Runtime with dtree ----------------------------------
while True:
    connection, address = server_socket.accept()
    print(f"Connected by: {address}")

    print(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), flush=True)

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

        rssi_dbm = session_dict["signal"]

        print("signal: " + rssi)

    # plot the dimensions and datatypes
    rssi = convert_rssi_to_value(rssi_dbm)
    y_pred = dtree.predict([rssi])
    print(y_pred)

    if y_pred == ['good']:
        y_pred = 1
    elif y_pred == ['bad']:
        y_pred = 0
    else:
        y_pred = 0.5
    print(y_pred)

    # send the prediction to the receiver
    # s.send(y_pred[0].encode('utf-8'))
    
    # TODO send lqe prediction & noise
    num_bytes = struct.pack("f", y_pred)
    
    s.sendall(num_bytes)

    time.sleep(1)


