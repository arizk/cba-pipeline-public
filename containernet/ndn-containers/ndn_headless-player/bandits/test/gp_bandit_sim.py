import numpy as np
import matplotlib.pyplot as plt
from sklearn import gaussian_process as gp
from packages.banditAlgorithms.GpBandit import GpBandit

# time horizon
T = 1000
# ucb width factor
beta = 1
# Dimensionality of the features
D = 3

# number of arms
K = 2

# initialize bandit object
sigma2 = 1
kernel_s = gp.kernels.ConstantKernel(constant_value=1)
kernel_z = gp.kernels.DotProduct(sigma_0=0)
kernel = gp.kernels.Product(kernel_s, kernel_z)
GPBanditObj = GpBandit(D, K, beta, kernel, sigma2)

# true regression coefficient for simulation
theta_a_true = [np.random.randn(D, 1) for _ in range(K)]

# save rewards
rew_op = np.zeros((T, 1))
rew_GPBandit = np.zeros((T, 1))

# save actions
a_GPBandit = np.zeros(T, dtype=np.int8)
a_opt = np.zeros(T, dtype=np.int8)
for t in range(T):
    print(t)
    # observe context
    x_a = np.random.randn(D, K)

    # find bandit action
    a_GPBandit[t] = GPBanditObj.get_opt_action(x_a)
    a_opt[t] = np.argmax([theta_a_true[a].T.dot(x_a[:, a].reshape(-1, 1)) for a in range(K)])

    # observe reward
    noise = np.random.randn(1)
    rew_GPBandit[t] = theta_a_true[a_GPBandit[t]].T.dot(x_a[:, a_GPBandit[t]].reshape(-1, 1)) + noise
    rew_op[t] = theta_a_true[a_opt[t]].T.dot(x_a[:, a_opt[t]].reshape(-1, 1)) + noise

    # update bandit parameters
    GPBanditObj.update_params(x_a, rew_GPBandit[t], a_GPBandit[t])

plt.figure()
plt.title("GpBandit")
plt.plot(np.cumsum(rew_op), label="optimal")
plt.plot(np.cumsum(rew_GPBandit), label="GpBandit")
plt.plot(np.cumsum(rew_op) - np.cumsum(rew_GPBandit), label="GpBandit-regret")
plt.xlabel("time")
plt.ylabel("cumulative reward")
plt.legend()
plt.show()
