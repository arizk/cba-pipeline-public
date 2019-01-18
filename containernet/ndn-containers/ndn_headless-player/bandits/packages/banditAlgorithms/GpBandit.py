import numpy as np
from sklearn import gaussian_process as gp


# TODO make beta_t time dependent
# TODO create separated GP obj per arm, since White noise kernel for the actions is assumed anyway
class GpBandit(object):
    """Creates a bandit object which uses the gaussian process contextual bandit algorithm.
    (https://papers.nips.cc/paper/4487-contextual-gaussian-process-bandit-optimization)
    """

    def __init__(self, D, K, beta_t, kernel, sigma2):
        """Initializes a GP contextual bandit object

                Parameters
                ----------
                D : int
                    Dimension of the context features
                K : int
                    Number of arms
                beta_t: float
                    width parameter of the ucb
                kernel: kernel obj
                    kernel for the gaussian processes from sklearn
                sigma2:
                    noise level of the observations in the GP
        """

        self.D = D
        self.K = K
        self.beta_t = beta_t
        # TODO: Make this varinputin for the kernel --> default use RBF kernel?
        self.GpObj = gp.GaussianProcessRegressor(kernel=kernel, alpha=sigma2)
        # initialization
        self.S = np.array([]).reshape(0, 1)
        self.Z = np.array([]).reshape(0, D)
        self.X = np.hstack((self.S, self.Z))
        self.y = np.array([]).reshape(0, 1)
        self.sigma2 = sigma2

    def get_opt_action(self, x_a):
        """Calculates action.

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
        (mu, sigma) = self.GpObj.predict(np.hstack((np.arange(self.K).reshape(-1, 1), x_a.T)), return_std=True)
        a_t = np.argmax(mu.T + self.beta_t * sigma)

        return a_t

    def update_params(self, x_a, r_t, a_t):
        """Updates the parameters.

                        Parameters
                        ----------
                        x_a
                            D x K matrix. D dimensional context feature vector for each arm a in (1,...,K)
                        r_t
                            last achieved reward when playing action a_t
                        a_t
                            last played arm


                        """
        self.S = np.vstack((self.S, a_t))
        self.Z = np.vstack((self.Z, x_a[:, a_t].reshape(1, -1)))
        self.y = np.vstack((self.y, r_t))
        self.X = np.hstack((self.S, self.Z))
        self.GpObj.fit(self.X, self.y)
