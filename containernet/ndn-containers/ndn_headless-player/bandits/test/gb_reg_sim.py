from packages.sparseEstimation.GbRegression import GbRegression as Gb
import numpy as np
import matplotlib.pyplot as plt


p = 100
n = 1000
beta = np.random.randn(p)
s_0 = 3
idx = np.random.choice(range(p), p - s_0, replace=False)
beta[idx] = 0

X = np.random.randn(n, p)
noise = np.random.randn(n).reshape(-1, 1)
y = X.dot(beta.reshape(-1, 1)) + noise

obj = Gb(1 / 2, 1 / 2, 1e-6, 1e-6, p)

#Batch
#obj.fit(X, y)
#SVI
obj.fit_svi(X,y)

print(beta)
print(obj.coef_)

print(np.mean((obj.coef_ - beta) ** 2))

plt.figure()
plt.stem(beta, linefmt='r')
plt.stem(obj.coef_, linefmt='b')
plt.show()
