import pandas as pd
from sklearn import base
from typing import List, Tuple
import multiprocessing as mp

TupleOrList = tuple([Tuple, List])

class PRR(base.BaseEstimator, base.TransformerMixin):
    """Calculate PRR based on `target`"""

    def __init__(self, source:str, window_size:int, ahead:int, target:str='prr'):
        self.source = source
        self.target = source if target is None else target

        if not isinstance(window_size, int) or not window_size > 0:
            raise ValueError(f'window_size should be positive integer. Got `{window_size}` instead.')

        self.window = window_size

        if not isinstance(ahead, int) or not ahead >= 0:
            raise ValueError(f'ahead should be greater or equal to zero integer. Got `{ahead}` instead.')

        self.ahead = ahead

    def fit(self, X: pd.DataFrame, y=None):
        return self

    def calculate_prr(self, dataframe):
        df = dataframe.copy()
        df[self.target] = (df[self.source].astype(bool).rolling(self.window).sum() / self.window).shift(-1 * self.window * self.ahead)
        return df

    def transform(self, X: pd.DataFrame, y='deprecated'):
        if isinstance(X, TupleOrList):
            with mp.Pool() as p:
                return p.map(self.calculate_prr, X)

        return self.calculate_prr(X)
    