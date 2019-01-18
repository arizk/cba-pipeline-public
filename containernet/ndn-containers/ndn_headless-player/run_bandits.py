#!/usr/bin/env python3

import os
import sys
import pickle
import numpy as np
import matplotlib.pyplot as plt
sys.path.append('/bandits')
from packages.banditAlgorithms.SparseBayesUcb import SparseBayesUcb
from packages.banditAlgorithms.SparseBayesUcbSvi import SparseBayesUcbSvi
from packages.banditAlgorithms.SparseBayesUcbOse import SparseBayesUcbOse
from packages.banditAlgorithms.LinucbBandit import LinucbBandit
import time
import math
import itertools
from scipy import signal
#import pickle
#import string
#import random

def choose(algo, context_vecs):
    #with open('/choose-context_vecs-{}'.format(random.choice(string.ascii_letters)), 'wb') as f:
    #    pickle.dump(context_vecs, f)
    # ucb width factor
    alpha = 1
    a = 1 / 2
    b = 1 / 2
    c_0 = 1e-6
    d_0 = 1e-6

    # Columns are arms, rows are features
    context_mat = preprocess(context_vecs).T
    #with open('/choose-context_mat-{}'.format(random.choice(string.ascii_letters)), 'wb') as f:
    #    pickle.dump(context_mat, f)
    # Load or instantiate the bandit
    try:
        with open('/{}.pkl'.format(algo), 'rb') as f:
            ucbObj = pickle.load(f)
    except IOError:
        if algo == 'sbu':
            ucbObj = SparseBayesUcb(context_mat.shape[0], context_mat.shape[1], a, b, c_0, d_0)
        elif algo == 'sbusvi':
            ucbObj = SparseBayesUcbSvi(context_mat.shape[0], context_mat.shape[1], a, b, c_0, d_0)
        elif algo == 'sbuose':
            ucbObj = SparseBayesUcbOse(context_mat.shape[0], context_mat.shape[1], a, b, c_0, d_0)
        elif algo == 'linucb':
            ucbObj = LinucbBandit(context_mat.shape[0], context_mat.shape[1], alpha)
        else:
            with open('py_err.log', 'w') as f:
                f.write("Error: invalid algorithm")
            return -1
    
    # Get the optimal policy
    action = ucbObj.get_opt_action(context_mat)
    
    # Write out the linUCB object so we can update it when we get the reward
    try:
        with open('/{}.pkl'.format(algo), 'wb') as f:
            pickle.dump(ucbObj, f)
    except IOError as e:
        with open('py_err.log', 'w') as f:
            f.write("Error dumping object: {0}\n".format(str(e))) 
        return -1

    return action

def update(algo, context_vecs, action, reward, timestep):
    timestep += os.getenv('BEGIN_TIMESTEP', 0)
    #with open('/update-context_vecs-{}'.format(random.choice(string.ascii_letters)), 'wb') as f:
    #    pickle.dump(context_vecs, f)
    # Columns are arms, rows are features
    context_mat = preprocess(context_vecs).T
    #with open('/update-context_mat-{}'.format(random.choice(string.ascii_letters)), 'wb') as f:
    #    pickle.dump(context_mat, f)
    # Load the UCB paramters and context
    try:
        with open('/{}.pkl'.format(algo), 'rb') as f:
            ucbObj = pickle.load(f)
    except IOError:
        return -1

    if algo == 'linucb':
        ucbObj.update_params(context_mat, reward, action)
    else:
        ucbObj.update_params(context_mat, reward, action, timestep)

    try:
        with open('/{}.pkl'.format(algo), 'wb') as f:
            pickle.dump(ucbObj, f)
        return 1
    except IOError:
        return -2

def preprocess(context_vecs):
    """
    Each arm should have 101 elements, and each section has a label before it:
    0: bufferFill
    1-50: RTT
    51-100: number of hops

    We probably won't get this size from Adaptation, so any vector with less than/more 
    than 50 elements gets up/down sampled. Additionally, the supplied vector has
    delimiters before each section.

    A better way would be to pass in a dictionary, but that didn't work when I tried it
    and I didn't want to waste any time.
    """
    # Split up the arguments
    context_dicts = []
    for arm in context_vecs:
        delim = 'DELIM'
        idx = 0
        idx_labels = {0: 'buffer', 1: 'rtt', 2: 'numHops'}
        arm_features = {'buffer': []}
        for elem in arm:
            if elem == delim:
                idx += 1
                arm_features[idx_labels[idx]] = []
            else:
                arm_features[idx_labels[idx]].append(elem)
        context_dicts.append(arm_features)
    # Up/downsample existing ones
    for arm in context_dicts:
        for feature in ['rtt', 'numHops']:
            if len(arm[feature]) > 0 and len(arm[feature]) != 50:
                arm[feature] = signal.resample(arm[feature], 50)
                arm[feature] = [round(x, 0) for x in arm[feature]]
                arm[feature] = np.array(arm[feature])
    # Fill in missing features
    # TODO implement ecn and numHops
    rttMat = np.array([x['rtt'] for x in context_dicts if len(x['rtt']) > 0])
    avgs = {}
    avgs['rtt'] = np.average(rttMat, axis=0)
    avgs['numHops'] = np.zeros(50)
    for arm in context_dicts:
        for feature in ['rtt', 'numHops']:
            if len(arm[feature]) == 0:
                arm[feature] = avgs[feature]
    # Everything's ready, now we build the return matrix
    context_mat = np.empty((len(context_vecs), 101))
    for i in range(len(context_vecs)):
        arm_dict = context_dicts[i]
        arm_vec = np.concatenate((arm_dict['buffer'], arm_dict['rtt'], arm_dict['numHops']))
        context_mat[i] = arm_vec
    return context_mat
