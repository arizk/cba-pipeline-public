import numpy as np
import matplotlib.pyplot as plt

from packages.banditAlgorithms.SparseBayesUcbOse import SparseBayesUcbOse

# time horizon
T = 1000
# Dimensionality of the features
D = 10

# number of arms
K = 10

a = 1 / 2
b = 1 / 2
c_0 = 1e-6
d_0 = 1e-6

# initialize bandit object
sparseBayesUcbObj = SparseBayesUcbOse(D, K, a, b, c_0, d_0)

# true regression coefficient for simulation
theta_a_true = [np.random.randn(D, 1) for _ in range(K)]
# make them sparse
s_0 = 5
for a in range(K):
    idx = np.random.choice(range(D), D - s_0, replace=False)
    theta_a_true[a][idx] = 0

# save rewards
rew_op = np.zeros((T, 1))
rew_sparseBayesUcb = np.zeros((T, 1))

# save actions
a_sparseBayesUcb = np.zeros(T, dtype=np.int8)
a_opt = np.zeros(T, dtype=np.int8)
for t in range(T):
    print(t)
    # observe context
    x_a = np.random.randn(D, K)

    # find bandit action
    a_sparseBayesUcb[t] = sparseBayesUcbObj.get_opt_action(x_a)
    a_opt[t] = np.argmax([theta_a_true[a].T.dot(x_a[:, a].reshape(-1, 1)) for a in range(K)])

    # observe reward
    noise = np.random.randn(1)
    rew_sparseBayesUcb[t] = theta_a_true[a_sparseBayesUcb[t]].T.dot(x_a[:, a_sparseBayesUcb[t]].reshape(-1, 1)) + noise
    rew_op[t] = theta_a_true[a_opt[t]].T.dot(x_a[:, a_opt[t]].reshape(-1, 1)) + noise

    # update bandit parameters
    sparseBayesUcbObj.update_params(x_a, rew_sparseBayesUcb[t], a_sparseBayesUcb[t], t)

plt.figure()
plt.title("SparseBayesUcb")
plt.plot(np.cumsum(rew_op), label="optimal")
plt.plot(np.cumsum(rew_sparseBayesUcb), label="SparseBayesUcb")
plt.plot(np.cumsum(rew_op) - np.cumsum(rew_sparseBayesUcb), label="SparseBayesUcb-regret")
plt.xlabel("time")
plt.ylabel("cumulative reward")
plt.legend()
plt.show()
