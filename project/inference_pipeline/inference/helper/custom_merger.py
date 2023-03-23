import pandas as pd
from sklearn import base
from typing import List, Tuple

TupleOrList = tuple([Tuple, List])

class CustomMerger(base.BaseEstimator, base.TransformerMixin):
    """Merge List of DataFrames"""

    def __init__(self):
        pass

    def fit(self, X: pd.DataFrame, y=None):
        return self

    def transform(self, X: pd.DataFrame, y='deprecated', copy=True):
        if isinstance(X, TupleOrList):
            return pd.concat(X, ignore_index=True).reset_index(drop=True)

        return X