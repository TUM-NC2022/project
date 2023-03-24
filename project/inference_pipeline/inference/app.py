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
sleep(10)
## socket
# create a socket object
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# get local machine name
host = socket.gethostbyname("host.docker.internal")  # Access the host ip
# set the port number to connect on
port = 5000
# connect to the receiving application
s.connect((host, port))


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


# multi-layer perceptron init
resample = over_sampling.RandomOverSampler()
scaler = preprocessing.StandardScaler()
mlp = pipeline.make_pipeline(
    scaler,
    resample,
    neural_network.MLPClassifier(
        hidden_layer_sizes=(
            100,
            100,
            100,
        ),
        activation="relu",
        solver="adam",
    ),
)

# decision tree init
features = ["rssi", "rssi_std", "rssi_avg"]
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

# use decision tree at runtime
while True:
    print(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), flush=True)
    # generate mock data
    rssi_mock = 1

    # plot the dimensions and datatypes of X_test
    print([X_test[0]])
    y_pred = dtree.predict([X_test[0]])
    print(y_pred)

    random_coice = random.choice([0, 0.5, 1])
    # send the prediction to the receiver
    # s.send(y_pred[0].encode('utf-8'))
    num_bytes = struct.pack("f", random_coice)
    s.sendall(num_bytes)

    time.sleep(10)
