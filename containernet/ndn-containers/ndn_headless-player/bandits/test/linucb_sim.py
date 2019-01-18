import numpy as np
import matplotlib.pyplot as plt

from packages.banditAlgorithms.LinucbBandit import LinucbBandit

# time horizon
T = 1000
# ucb width factor
alpha = 1
# Dimensionality of the features
D = 100

# number of arms
K = 3

# initialize bandit object
linucbObj = LinucbBandit(D, K, alpha)

# true regression coefficient for simulation
theta_a_true = [np.random.randn(D, 1) for _ in range(K)]

# save rewards
rew_op = np.zeros((T, 1))
rew_linucb = np.zeros((T, 1))

# save actions
a_linucb = np.zeros(T, dtype=np.int8)
a_opt = np.zeros(T, dtype=np.int8)
for t in range(T):
    # observe context
    x_a = np.random.randn(D, K)

    # find bandit action
    a_linucb[t] = linucbObj.get_opt_action(x_a)
    a_opt[t] = np.argmax([theta_a_true[a].T.dot(x_a[:, a].reshape(-1, 1)) for a in range(K)])

    # observe reward
    noise = np.random.randn(1)
    rew_linucb[t] = theta_a_true[a_linucb[t]].T.dot(x_a[:, a_linucb[t]].reshape(-1, 1)) + noise
    rew_op[t] = theta_a_true[a_opt[t]].T.dot(x_a[:, a_opt[t]].reshape(-1, 1)) + noise

    # update bandit parameters
    linucbObj.update_params(x_a, rew_linucb[t], a_linucb[t])

plt.figure()
plt.title("Linucb")
plt.plot(np.cumsum(rew_op), label="optimal")
plt.plot(np.cumsum(rew_linucb), label="linucb")
plt.plot(np.cumsum(rew_op) - np.cumsum(rew_linucb), label="linucb-regret")
plt.xlabel("time")
plt.ylabel("cumulative reward")
plt.legend()
plt.show()
