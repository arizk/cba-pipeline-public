import numpy as np
import matplotlib.pyplot as plt

from packages.banditAlgorithms.LassoBandit import LassoBandit

# time horizon
T = 2000
# lasso Bandit parameters
q = 1  # forced sampling parameter

h = 5  # localization parameter

lambda_1 = 0.05
lambda_2_0 = 0.05

# Dimensionality of the features
D = 100

# number of arms
K = 2

# initialize bandit object
lassoBanditObj = LassoBandit(D, K, q, h, lambda_1, lambda_2_0)

# true regression coefficient for simulation
theta_a_true = [np.random.randn(D, 1) for _ in range(K)]
# make them sparse
s_0 = 5
for a in range(K):
    idx = np.random.choice(range(D), D - s_0, replace=False)
    theta_a_true[a][idx] = 0

# save rewards
rew_op = np.zeros((T, 1))
rew_lassoBandit = np.zeros((T, 1))

# save actions
a_lassoBandit = np.zeros(T, dtype=np.int8)
a_opt = np.zeros(T, dtype=np.int8)
for t in range(T):
    print(t)
    # observe context truncate at 1/-1
    x = np.random.randn(D, 1)
    x[x > 1] = 1
    x[x < -1] = -1

    # find bandit action
    a_lassoBandit[t] = lassoBanditObj.get_opt_action(x, t)
    a_opt[t] = np.argmax([theta_a_true[a].T.dot(x) for a in range(K)])

    # observe reward
    noise = np.random.randn(1)
    rew_lassoBandit[t] = theta_a_true[a_lassoBandit[t]].T.dot(x) + noise
    rew_op[t] = theta_a_true[a_opt[t]].T.dot(x) + noise

    # update bandit parameters
    lassoBanditObj.update_params(x, rew_lassoBandit[t], a_lassoBandit[t], t)

plt.figure()
plt.title("Lasso Bandit")
plt.plot(np.cumsum(rew_op), label="optimal")
plt.plot(np.cumsum(rew_lassoBandit), label="LassoBandit")
plt.plot(np.cumsum(rew_op) - np.cumsum(rew_lassoBandit), label="LassoBandit-regret")
plt.xlabel("time")
plt.ylabel("cumulative reward")
plt.legend()
plt.show()
