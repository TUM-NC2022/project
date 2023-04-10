from typing import List, Tuple
import multiprocessing as mp
import pandas as pd
from sklearn import base
import numpy as np

def interpolate_with_gaussian_noise(data: pd.Series) -> pd.Series:
    """Couldn't find a proper name. Very slow ..."""
    DTYPE = np.float32

    series = data.astype(DTYPE)
    values = series.tolist()
    processed = []

    series_size = len(values)

    prev_rssi = 0
    prev_seq = -1
    for seq, rssi in enumerate(values):
        if not np.isnan(rssi):
            avg_rssi = np.mean([prev_rssi, rssi])
            std_rssi = np.std([prev_rssi, rssi])
            std_rssi = std_rssi if std_rssi > 0 else np.nextafter(DTYPE(0), DTYPE(1))
            diff = seq - prev_seq - 1

            processed.extend(np.random.normal(avg_rssi, std_rssi, size=diff))
            processed.append(rssi)
            prev_seq, prev_rssi = seq, rssi

    avg_rssi = np.mean([prev_rssi, 0.])
    std_rssi = np.std([prev_rssi, 0.])
    diff = series_size - prev_seq - 1
    processed.extend(np.random.normal(avg_rssi, std_rssi, size=diff))

    series = pd.Series(data=processed, index=data.index, dtype=DTYPE)
    return series

def interpolate_with_constant(data: pd.Series, constant: int = 0) -> pd.Series:
    """Interpolate missing values with constant value."""
    return data.fillna(value=constant)

class CustomInterpolation(base.BaseEstimator, base.TransformerMixin):
    """Custom interpolation function to be used in"""
    
    STRATEGIES_ALL = ['none', 'gaussian', 'constant']

    def __init__(self, source:str, strategy:str='constant', constant:float=0, target=None):
        if strategy not in self.STRATEGIES_ALL:
            raise ValueError(f'"{strategy}" is not available strategy')

        self.strategy = strategy
        self.constant = constant

        self.source = source
        self.target = source if target is None else target


    def with_constant(self, data:pd.DataFrame) -> pd.DataFrame:
        df = data.copy()
        df[self.target] = df[self.source].fillna(value=self.constant)
        return df

    def with_gaussian(self, data:pd.DataFrame) -> pd.DataFrame:
        df = data.copy()
        df[self.target] = interpolate_with_gaussian_noise(df[self.source])
        return df
    
    def with_none(self, data:pd.DataFrame) -> pd.DataFrame:
        df = data.copy()
        src = [self.source] if isinstance(self.source, [str]) else self.source
        df = df.dropna(subset=src)
        return df

    def do_interpolation(self, X:pd.DataFrame) -> pd.DataFrame:
        if self.strategy == 'constant':
            return self.with_constant(X)

        if self.strategy == 'gaussian':
            return self.with_gaussian(X)
        
        if self.strategy == 'none':
            return self.with_none(X)

        raise ValueError(f'"{self.strategy}" is not available strategy')

    def fit(self, X: pd.DataFrame, y=None):
        return self

    def transform(self, X, y='deprecated', copy=True):
        if isinstance(X, (List, Tuple, )):
            with mp.Pool() as p:
                return p.map(self.do_interpolation, X)

        return self.do_interpolation(X)
    
