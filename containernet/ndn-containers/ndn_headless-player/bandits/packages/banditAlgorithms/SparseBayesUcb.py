import numpy as np
from packages.sparseEstimation.GbRegression import GbRegression as Gb
from scipy.special import erfinv


# This is relative straight forward
class SparseBayesUcb(object):
    """Creates a bandit object which uses the gb contextual bandit algorithm with batch variational bayes.
    """

    def __init__(self, D, K, a, b, c_0, d_0):
        """Initializes a GB contextual bandit object

                Parameters
                ----------
                D : int
                    Dimension of the context features
                K : int
                    Number of arms
                a : float
                    hyper parameter for the TPBN
                b : float
                    hyper parameter for the TPBN
                c_0 : float
                    hyper parameter for the TPBN
                d_0 : float
                    hyper parameter for the TPBN
        """

        self.D = D
        self.K = K
        self.GbObj = [Gb(a, b, c_0, d_0, D) for _ in range(K)]
        self.X = [np.array([]).reshape(0, D)] * K
        self.y = [np.array([]).reshape(0, 1)] * K
        self.beta_t = 1e6

    def get_opt_action(self, x_a):
        """Calculates SparseBayesUcb action.

                Parameters
                ----------
                x_a
                    D x K matrix. D dimensional context feature vector for each arm a in (1,...,K)

                Returns
                -------
                int
                    arm index for the current parameter and context set_with_var

                """

        # estimate!
        mu = np.zeros(self.K)
        sigma = np.zeros(self.K)
        for a in range(self.K):
            (mu[a], sigma[a]) = self.GbObj[a].predict(x_a[:, a], return_std=True)

        a_t = np.argmax(mu + self.beta_t * sigma)

        return a_t

    def update_params(self, x_a, r_t, a_t, t):
        """Updates the parameters.

                        Parameters
                        ----------
                        x_a
                            D x K matrix. D dimensional context feature vector for each arm a in (1,...,K)
                        r_t
                            last achieved reward when playing action a_t
                        a_t
                            last played arm
                        t
                            time index of the bandit process

                        """

        self.X[a_t] = np.append(self.X[a_t], x_a[:, a_t].reshape(1, -1), axis=0).reshape(-1, self.D)
        self.y[a_t] = np.append(self.y[a_t], r_t).reshape(-1, 1)
        self.GbObj[a_t].fit(self.X[a_t], self.y[a_t])
        self.beta_t = np.sqrt(2) * erfinv(1 - 2 / (t + 1))
