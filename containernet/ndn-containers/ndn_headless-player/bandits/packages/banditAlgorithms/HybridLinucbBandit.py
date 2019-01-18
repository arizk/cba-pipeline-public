import numpy as np
from scipy.linalg import *


class HybridLinucbBandit(object):
    """Creates a bandit object which uses the hybrid linucb algorithm.
       (https://arxiv.org/pdf/1003.0146.pdf)
      """

    def __init__(self, D, K, k_indp, alpha):
        """Initializes a hybrid linucb contextual bandit object

                        Parameters
                        ----------
                        D : int
                            Dimension of the arm dependent context features
                        K : int
                            Number of arms
                        k_indp : int
                            Dimension of the arm independent context features
                        alpha:
                            confidence bound width parameter
                """
        self.D = D
        self.K = K
        self.k = k_indp

        # initialization
        self.A_0 = np.eye(k_indp)
        self.b_0 = np.zeros((k_indp, 1))
        self.A_a = [np.eye(D)] * K
        self.A_a_inv = self.A_a
        self.B_a = [np.zeros((D, k_indp))] * K
        self.b_a = [np.zeros((D, 1))] * K
        self.alpha = alpha

    def get_opt_action(self, x_a, z_a):
        """Calculates action.

                Parameters
                ----------
                x_a
                    D x K matrix. D dimensional arm dependent context feature vector for each arm a in (1,...,K)
                z_a
                    k x K matrix. k dimensional arm independent context feature vector for each arm a in (1,...,K)

                Returns
                -------
                int
                    arm index for the current parameter and context set_with_var

                """

        # estimate!
        A_0_inv = inv(self.A_0)
        beta_hat = A_0_inv.dot(self.b_0)
        theta_a_hat = [np.zeros((self.D, 1))] * self.K
        s_a = np.zeros((self.K, 1))
        p_t_a = np.zeros((self.K, 1))
        for a in range(self.K):
            theta_a_hat[a] = self.A_a_inv[a].dot(self.b_a[a] - self.B_a[a].dot(beta_hat))

            s_a[a] = (z_a[:, a].reshape(-1, 1).T.dot(A_0_inv).dot(z_a[:, a].reshape(-1, 1))
                      - 2 * z_a[:, a].reshape(-1, 1).T.dot(A_0_inv).dot(self.B_a[a].T).dot(self.A_a_inv[a]).dot(
                        x_a[:, a].reshape(-1, 1))
                      + x_a[:, a].reshape(-1, 1).T.dot(self.A_a_inv[a]).dot(x_a[:, a].reshape(-1, 1))
                      + x_a[:, a].reshape(-1, 1).T.dot(self.A_a_inv[a]).dot(self.B_a[a]).dot(A_0_inv).dot(
                        self.B_a[a].T).dot(
                        self.A_a_inv[a]).dot(x_a[:, a].reshape(-1, 1)))

            p_t_a[a] = z_a[:, a].reshape(-1, 1).T.dot(beta_hat) + x_a[:, a].reshape(-1, 1).T.dot(
                theta_a_hat[a]) + self.alpha * np.sqrt(
                s_a[a])

        a_t = np.argmax(p_t_a)

        return a_t

    def update_params(self, x_a, z_a, r_t, a_t):
        """Updates the parameters.

                Parameters
                ----------
                x_a
                    D x K matrix. D dimensional arm dependent context feature vector for each arm a in (1,...,K)
                z_a
                    k x K matrix. k dimensional arm independent context feature vector for each arm a in (1,...,K)
                r_t
                    last achieved reward when playing action a_t
                a_t
                    last played arm


                """

        x_a_t = x_a[:, a_t].reshape(-1, 1)
        z_a_t = z_a[:, a_t].reshape(-1, 1)
        self.A_0 = self.A_0 + self.B_a[a_t].T.dot(self.A_a_inv[a_t]).dot(self.B_a[a_t])
        self.b_0 = self.b_0 + self.B_a[a_t].T.dot(self.A_a_inv[a_t]).dot(self.b_a[a_t])

        self.A_a[a_t] = self.A_a[a_t] + x_a_t.dot(x_a_t.T)
        self.A_a_inv[a_t] = inv(self.A_a[a_t])
        self.B_a[a_t] = self.B_a[a_t] + x_a_t.dot(z_a_t.T)

        self.b_a[a_t] = self.b_a[a_t] + r_t * x_a_t

        self.A_0 = self.A_0 + z_a_t.dot(z_a_t.T) - self.B_a[a_t].T.dot(self.A_a_inv[a_t]).dot(
            self.B_a[a_t])

        self.b_0 = self.b_0 + r_t * z_a_t - self.B_a[a_t].T.dot(self.A_a_inv[a_t]).dot(self.b_a[a_t])
