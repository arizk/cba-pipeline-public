import numpy as np
from scipy.linalg import *


class LinucbBandit(object):
    """Creates a bandit object which uses the linucb algorithm.
    (https://arxiv.org/pdf/1003.0146.pdf)
    """

    def __init__(self, D, K, alpha):
        """Initializes a linucb contextual bandit object

                Parameters
                ----------
                D : int
                    Dimension of the context features
                K : int
                    Number of arms
                alpha:
                    confidence bound width parameter
        """

        self.D = D
        self.K = K
        self.alpha = alpha

        # initialization
        self.A_a = [np.eye(D)] * K
        self.A_a_inv = self.A_a
        self.b_a = [np.zeros((D, 1))] * K
        self.theta_a = [np.zeros((D, 1))] * K

    def get_opt_action(self, x_a):
        """Calculates linucb action.

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
        for a in range(self.K):
            self.theta_a[a] = self.A_a_inv[a].dot(self.b_a[a])

        p_t = np.zeros(self.K)
        for a in range(self.K):
            p_t[a] = self.theta_a[a].T.dot(x_a[:, a].reshape(-1, 1)) \
                     + self.alpha * np.sqrt(
                x_a[:, a].reshape(-1, 1).T.dot(self.A_a_inv[a].dot(x_a[:, a].reshape(-1, 1))))

        a_t = np.argmax(p_t)

        return a_t

    def update_params(self, x_a, r_t, a_t):
        """Updates the linucb parameters.

                        Parameters
                        ----------
                        x_a
                            D x K matrix. D dimensional context feature vector for each arm a in (1,...,K)
                        r_t
                            last achieved reward when playing action a_t
                        a_t
                            last played arm


                        """
        self.A_a[a_t] = self.A_a[a_t] + x_a[:, a_t].reshape(-1, 1).dot(x_a[:, a_t].reshape(-1, 1).T)
        self.A_a_inv[a_t] = inv(self.A_a[a_t])
        self.b_a[a_t] = self.b_a[a_t] + x_a[:, a_t].reshape(-1, 1) * r_t
