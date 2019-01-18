import numpy as np
from sklearn import linear_model as lm


class LassoBandit(object):
    """Creates a bandit object which uses the lasso bandit algorithm.
    (http://web.stanford.edu/~bayati/papers/LassoBandit.pdf)
    """

    def __init__(self, D, K, q, h, lambda_1, lambda_2_0):
        """Initializes a lasso bandit contextual bandit object

                Parameters
                ----------
                D : int
                    Dimension of the context features
                K : int
                    Number of arms
                q: int
                   forced sampling parameter
                h: float
                   localization parameter

                lambda_1: float
                    regularization parameter 1

                lambda_2_0: float
                    regularization parameter 2

        """

        self.D = D
        self.K = K
        self.q = q
        self.h = h
        self.lambda_1 = lambda_1
        self.lambda_2_0 = lambda_2_0
        self.lambda_2 = lambda_2_0

        self.T_a = [np.arange(q * a + 1 - 1, q * (a + 1) + 1 - 1) for a in range(K)]
        self.j_a = [np.arange(q * a + 1 - 1, q * (a + 1) + 1 - 1) for a in range(K)]
        self.n = 0

        self.S_a = [[]] * K
        self.X = np.array([]).reshape(0, D)
        self.y = np.array([]).reshape(0, 1)

        self.beta_hat_1 = [np.zeros((D, 1))] * K
        self.beta_hat_2 = [np.zeros((D, 1))] * K

    def get_opt_action(self, x, t):
        """Calculates action.

                Parameters
                ----------
                x
                    D x 1 vector. D dimensional context feature vector
                t
                    Current time index

                Returns
                -------
                int
                    arm index for the current parameter and context set_with_var

                """

        # estimate!
        a_forced = self.is_in_forced_sample_set(t)
        if a_forced == -1:
            K_hat = []
            for a in range(self.K):
                if x.T.dot(self.beta_hat_1[a]) > np.max(
                        [x.T.dot(self.beta_hat_1[a]) for a in range(self.K)]) - self.h / 2:
                    K_hat.append(a)
            p_t = np.zeros(self.K)
            for a in range(self.K):
                p_t[a] = x.T.dot(self.beta_hat_2[a])

            a_t = np.argmax(p_t[K_hat])

        else:
            a_t = a_forced

        return a_t

    def update_params(self, X_t, r_t, a_t, t):
        """Updates the parameters.

                        Parameters
                        ----------
                        X_t
                             D dimensional context feature vector
                        r_t
                            last achieved reward when playing action a_t
                        a_t
                            last played arm
                        t
                            Current time index

                        """

        self.X = np.append(self.X, X_t.reshape(1, self.D), axis=0)
        self.y = np.append(self.y, r_t)
        self.S_a[a_t] = np.append(self.S_a[a_t], t)
        self.lambda_2 = self.lambda_2_0 * np.sqrt((np.log(t + 1) + np.log(self.D)) / (t + 1))

        # estimate beta_hat_1, beta_hat_2
        for a in range(self.K):
            idx = self.T_a[a]
            idx = idx[idx <= t]

            # TODO: Check for initialization of sklearn lasso
            if np.size(idx) > 0:
                reg_lasso_1 = lm.Lasso(alpha=self.lambda_1, fit_intercept=False)
                reg_lasso_1.fit(self.X[idx, :], self.y[idx])
                self.beta_hat_1[a] = reg_lasso_1.coef_

        idx = np.int8(self.S_a[a_t])
        if np.size(idx) > 0:
            reg_lasso_2 = lm.Lasso(alpha=self.lambda_2, fit_intercept=False)
            reg_lasso_2.fit(self.X[idx, :], self.y[idx])
            self.beta_hat_2[a_t] = reg_lasso_2.coef_

    def is_in_forced_sample_set(self, t):
        """Checks if the time index is in the forced sample set_with_var.

                    Parameters
                    ----------
                    t: int
                        Current time index

                    Returns
                    -------
                    a_out: int
                        arm index for the corresponding forced sample set_with_var.
                            -1 if not in forced sample set_with_var
                    """

        # noinspection PyTypeChecker
        while t > max(max(self.T_a, key=tuple)):
            self.n = self.n + 1
            for a in range(self.K):
                self.T_a[a] = np.append(self.T_a[a], (2 ** self.n - 1) * self.K * self.q + self.j_a[a])

        a_out = -1
        for a in range(self.K):
            if np.isin(t, self.T_a[a]):
                a_out = a

        return a_out
