from helper.custom_interpolation import CustomInterpolation
from helper.custom_merger import CustomMerger
from helper.prr import PRR
from helper.synthetic_features import SyntheticFeatures
from sklearn import preprocessing, tree, model_selection
from imblearn import pipeline, over_sampling
import numpy as np
from datasets.trace1_Rutgers.transform import get_traces as load_rutgers, dtypes
import joblib
from typing import List, Tuple
import pandas as pd
from sklearn.model_selection import train_test_split
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
# save dtree to a file
joblib.dump(dtree, "dtree.joblib")
print("saved model")
# y_pred = model_selection.cross_val_predict(dtree, X[features].values, y, cv=cv, n_jobs=-1)
# print(imetrics.classification_report_imbalanced(y, y_pred, labels=labels))

def convert_rssi_to_value(rssi):
    if rssi < -127:
        return 128  # Error
    else:
        value = int((127/(-30)) * rssi + 127)
        return value if value >= 0 else 0