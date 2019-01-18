import numpy as np
import matplotlib.pyplot as plt

from packages.banditAlgorithms.HybridLinucbBandit import HybridLinucbBandit

# time horizon
T = 1000
# ucb width factor
alpha = 1
# Dimensionality of the features
D = 100

# Dimensionality of arm independent features
k = 50

# number of arms
K = 3

# initialize bandit object
hybridLinucbObj = HybridLinucbBandit(D, K, k, alpha)

# true regression coefficient for simulation
theta_a_true = [np.random.randn(D, 1) for _ in range(K)]
beta_true = np.random.randn(k, 1)

# save rewards
rew_op_hyb = np.zeros((T, 1))
rew_hyblinucb = np.zeros((T, 1))

a_hyblinucb = np.zeros(T, dtype=np.int8)
a_opt = np.zeros(T, dtype=np.int8)

for t in range(T):
    # observe context
    x_a = np.random.randn(D, K)
    z_a = np.random.randn(k, K)

    # find bandit action
    a_hyblinucb[t] = hybridLinucbObj.get_opt_action(x_a, z_a)
    a_opt[t] = np.argmax(
        [theta_a_true[a].T.dot(x_a[:, a].reshape(-1, 1)) + beta_true.T.dot(z_a[:, a].reshape(-1, 1)) for a in
         range(K)])

    # observe reward
    noise = np.random.randn(1)
    rew_hyblinucb[t] = beta_true.T.dot(z_a[:, a_hyblinucb[t]].reshape(-1, 1)) + theta_a_true[a_hyblinucb[t]].T.dot(
        x_a[:, a_hyblinucb[t]].reshape(-1, 1)) + noise

    rew_op_hyb[t] = beta_true.T.dot(z_a[:, a_opt[t]].reshape(-1, 1)) + theta_a_true[a_opt[t]].T.dot(
        x_a[:, a_opt[t]].reshape(-1, 1)) + noise

    # update bandit parameters
    hybridLinucbObj.update_params(x_a, z_a, rew_hyblinucb[t], a_hyblinucb[t])

plt.figure()
plt.title("hybrid Linucb")
plt.plot(np.cumsum(rew_op_hyb), label="optimal")
plt.plot(np.cumsum(rew_hyblinucb), label="hyblinucb")
plt.plot(np.cumsum(rew_op_hyb) - np.cumsum(rew_hyblinucb), label="hyblinucb-regret")
plt.xlabel("time")
plt.ylabel("cumulative reward")
plt.legend()
plt.show()
