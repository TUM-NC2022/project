import pandas as pd
from sklearn import base
from typing import List, Tuple
import multiprocessing as mp

class SyntheticFeatures(base.BaseEstimator, base.TransformerMixin):
    """Rolling window for mean & std features."""

    def __init__(self, source:str, window_size:int=10, target=None):
        self.source = source
        self.target = source if target is None else target

        if not isinstance(window_size, int) or not window_size > 0:
            raise ValueError(f'Window should be positive integer. Got `{window_size}` instead.')

        self.window = window_size

    def fit(self, X, y=None):
        return self

    def do_synthetics(self, data:pd.DataFrame) -> pd.DataFrame:
        df = data.copy()
        df[f'{self.target}_avg'] = df[self.source].rolling(self.window).mean()
        df[f'{self.target}_std'] = df[self.source].rolling(self.window).std()
        return df


    def transform(self, X: pd.DataFrame, y='deprecated', copy=True):
        if isinstance(X, (List, Tuple, )):
            with mp.Pool() as p:
                return p.map(self.do_synthetics, X)

        return self.do_synthetics(X)
    
    
