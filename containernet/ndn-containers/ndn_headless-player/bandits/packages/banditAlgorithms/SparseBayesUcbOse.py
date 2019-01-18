import numpy as np
from packages.sparseEstimation.GbRegression import GbRegression as Gb
from scipy.special import erfinv


# This should be very fast because it does not store the design matrix,
# and does not need to compute large linear products
class SparseBayesUcbOse(object):
    """Creates a bandit object which uses the gb contextual bandit algorithm with one sample estimate.
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
        self.N = [np.zeros(1)] * K
        # self.X = [np.array([]).reshape(0, D)] * K
        # self.y = [np.array([]).reshape(0, 1)] * K
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



        self.N[a_t] = self.N[a_t] + 1
        tau = 0
        kappa = 1
        rho_t_a_t = (self.N[a_t] + tau) ** (-kappa)
        self.GbObj[a_t].svi_update(x_a[:, a_t].reshape(1, -1), np.array(r_t).reshape(-1, 1), rho_t_a_t, self.N[a_t])
        self.beta_t = np.sqrt(2) * erfinv(1 - 2 / (t + 1))
