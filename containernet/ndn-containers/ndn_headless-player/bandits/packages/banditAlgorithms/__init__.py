"""banditAlgorithms module with multiple contextual bandit algorithms.

Example
-------

Implemented Algorithms
----------------------

"""
from packages.banditAlgorithms import HybridLinucbBandit
from packages.banditAlgorithms import LinucbBandit
from packages.banditAlgorithms import LassoBandit
from packages.banditAlgorithms import GpBandit
from packages.banditAlgorithms import SparseBayesUcb
from packages.banditAlgorithms import SparseBayesUcbSvi
from packages.banditAlgorithms import SparseBayesUcbOse

__all__ = ["HybridLinucbBandit.py", "LinucbBandit.py", "LassoBandit.py", "GpBandit.py", "SparseBayesUcb.py",
           "SparseBayesUcbSvi.py", "SparseBayesUcbOse.py"]
