from helper.custom_interpolation import CustomInterpolation
from helper.custom_merger import CustomMerger
from helper.prr import PRR
from helper.synthetic_features import SyntheticFeatures
from sklearn import preprocessing, tree, model_selection
from imblearn import pipeline, over_sampling
import numpy as np
from datasets.trace1_Rutgers.transform import get_traces as load_rutgers
import joblib
from typing import List
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

features = ["rssi", "rssi_avg"]

dtree = pipeline.Pipeline(
    [
        ("scaler", preprocessing.StandardScaler()),
        ("resample", over_sampling.RandomOverSampler(random_state=SEED)),
        ("dtree", tree.DecisionTreeClassifier(max_depth=4, random_state=SEED)),
    ]
)

df = prepare_data()

# This is used for cross-validation. Typically used when observing performance of algorithm
cv = model_selection.StratifiedKFold(n_splits=3, shuffle=True, random_state=SEED)

dataset = df.replace([np.inf, -np.inf], np.nan).dropna(subset=features)

X, y = df.drop(["class"], axis=1), df["class"].ravel()

#Split the dataset into training and testing sets
X_train, X_test, y_train, y_test = train_test_split(
    X[features].values, y, stratify=y, random_state=10
)

#train the decision tree
dtree.fit(X_train, y_train)
score = dtree.score(X_test, y_test)

print("score: " + str(score))

# save dtree to a file
joblib.dump(dtree, "dtree.joblib")
print("-> saved model to file")