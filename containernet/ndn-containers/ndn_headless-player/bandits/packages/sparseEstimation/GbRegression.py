import numpy as np
from scipy.linalg import *
from scipy.special import kv, gammaln


# TODO implement jeffreys prior c_0=0 d_0=0
# TODO implement gibbs sampling
# TODO implement sampling from variational distributions
# TODO add maxiter
class GbRegression(object):
    """Creates a generalized beta regression object which uses the TPBN model for sparse bayesian regression.
    (http://papers.nips.cc/paper/4439-generalized-beta-mixtures-of-gaussians)
    """

    def __init__(self, a, b, c_0, d_0, p):
        """Initializes a TPBN regression object

                Parameters
                ----------
                a : float
                    hyper parameter
                b : float
                    hyper parameter
                c_0 : float
                    hyper parameter
                d_0 : float
                    hyper parameter
                p:
                    number of dimensions in the regression
            """
        # hyper parameters
        self.hyper = HyperParam(a, b, c_0, d_0)

        # regression parameters
        self.p = p
        self.n_train = 1
        self.X_train = np.array([])
        self.X_T_X = np.array([])
        self.y_train = np.array([])
        self.coef_ = np.zeros((self.p, 1))
        self.sigma_ = np.eye(self.p)

        # VB initialization
        self.var = VariationalParam(self.p, self.hyper)
        self.nat = VariationalNatParam(self.p, self.hyper)
        self.nat.set_with_var(self.p, self.hyper, self.var)
        self.nat_hat = NatEstimate(self.nat)
        self.elbo = -1e10

    def fit(self, X, y, tol=1e-2):
        """fit the regression model using variational Bayes.

                Parameters
                ----------
                X
                    n x p matrix. Design matrix with n samples and p variables
                y
                    n x 1 vector. response vector of the regression
                tol: float
                    tolerance value for convergence of the elbo

                """
        self.preprocess_data(X,y)
        self.X_T_X = self.X_train.T.dot(self.X_train)

        # update elbo with traing data information
        self.elbo = self.compute_lower_bound()

        # VB!
        converged = False
        # TODO: change convergence to absulte
        while np.logical_not(converged):
            elbo_old = self.elbo
            # TODO Check of cholesky factorization is possible here
            self.var.Sigma_inv = self.var.sigma2_inv * (self.X_T_X + self.var.T_inv)
            self.var.A_inv = inv(self.X_T_X + self.var.T_inv)

            self.var.mu = self.var.A_inv.dot(self.X_train.T).dot(self.y_train)
            self.var.Sigma = 1 / self.var.sigma2_inv * self.var.A_inv
            self.var.bb = self.var.Sigma + self.var.mu.dot(self.var.mu.T)

            self.var.c_ast = (self.n_train + self.p + self.hyper.c_0) / 2
            self.var.d_ast = (self.y_train.T.dot(self.y_train)
                              - 2 * self.y_train.T.dot(self.X_train).dot(self.var.mu)
                              + np.sum(self.var.bb * self.X_T_X.T)
                              + np.diag(self.var.bb).reshape(-1, 1).T.dot(self.var.tau_inv)
                              + self.hyper.d_0) / 2

            self.var.sigma2_inv = self.var.c_ast / self.var.d_ast

            self.var.lambda_ = (self.hyper.a + self.hyper.b) / (self.var.tau + self.var.phi)

            self.var.phi = (self.p * self.hyper.b + 1 / 2) / (self.var.omega + np.sum(self.var.lambda_))

            self.var.omega = 1 / (self.var.phi + 1)

            self.var.v = np.sqrt(2 * self.var.lambda_ * self.var.sigma2_inv * np.diag(self.var.bb).reshape(-1, 1))

            self.var.tau = ((np.sqrt(self.var.sigma2_inv * np.diag(self.var.bb).reshape(-1, 1))
                             * kv(self.hyper.a + 1 / 2, self.var.v)) /
                            (np.sqrt(2 * self.var.lambda_) * kv(self.hyper.a - 1 / 2, self.var.v)))

            self.var.tau_inv = ((np.sqrt(2 * self.var.lambda_) * kv(3 / 2 - self.hyper.a, self.var.v)) /
                                (np.sqrt(self.var.sigma2_inv * np.diag(self.var.bb).reshape(-1, 1))
                                 * kv(1 / 2 - self.hyper.a, self.var.v)))

            self.var.T_inv = np.diag(np.squeeze(self.var.tau_inv))

            self.elbo = self.compute_lower_bound()
            elbo_new = self.elbo

            eps = np.abs(elbo_new - elbo_old)
            if eps < tol:
                converged = True

        self.coef_ = np.squeeze(self.var.mu)
        self.sigma_ = self.var.Sigma

    # test
    def fit_svi(self, X, y, tol=1e-2, kappa=1, tau=0):
        """fit the regression model using stochastic variational inference.

                   Parameters
                   ----------
                   X
                       n x p matrix. Design matrix with n samples and p variables
                   y
                       n x 1 vector. response vector of the regression
                   tol: float
                       tolerance value for convergence of the elbo
                   kappa: float
                        scalar kappa in (.5,1]. Forgetting rate for the step size
                   tau: float
                        scalar tau>0. Delay for the step size

                   """

        self.preprocess_data(X, y)

        converged = False
        # TODO: change convergence to absulte
        t = 0
        while np.logical_not(converged):

            elbo_old = self.compute_lower_bound()

            idx = np.random.choice(range(self.n_train), 1)
            X_sam = self.X_train[idx, :].reshape(1, -1)
            y_sam = self.y_train[idx, :].reshape(1, -1)
            rho_t = (t + 1 + tau)**(-kappa)

            self.svi_update(X_sam, y_sam, rho_t, self.n_train)

            elbo_new = self.elbo
            eps = np.abs(elbo_new - elbo_old)
            if eps < tol:
                converged = True
            t = t + 1

    def svi_update(self, X_sam, y_sam, rho_t, N):
        """updates the variational parameter with one step in the direction of the natural gradient.

                     Parameters
                     ----------
                     X_sam
                         n_mini x p matrix. minibatch design matrix with n samples and p variables
                     y_sam
                         n_mini x 1 vector. minibatch response vector of the regression
                     rho_t
                        scalar. Step size for the stochastic gradient
                     N
                        number of replicates to calculate an unbiased estimate of the gradient

                     """
        X_T_X_sam = X_sam.T.dot(X_sam)

        # Calculate unbiased estimate of the natural gradient
        self.nat_hat.estimate_nat(self.hyper, self.var, X_sam, X_T_X_sam, y_sam, N, self.p)
        # calculate updates
        self.nat.gradient_update(self.nat_hat, rho_t)
        # calculate parameters
        self.var = self.nat.get_normal_param(self.p, self.hyper)
        self.elbo = self.compute_lower_bound()

        self.coef_ = np.squeeze(self.var.mu)
        self.sigma_ = self.var.Sigma

    def predict(self, X_ast, return_std=False, return_cov=False):
        """ predicts the points from X_ast in posterior mean and if specified with bayesian confidence interval

        Parameters
        ----------
        X_ast:
            n x p matrix. n points with p dimensions for the prediction
        return_std: bool
            if True returns standard deviation of the query points
        return_cov: bool
            if True returns covariance matrix of the query points
        Returns
        -------

        """
        dim = np.shape(X_ast)
        if np.shape(dim)[0] == 1:
            n_ast = 1
        else:
            n_ast = dim[0]

        mu = X_ast.dot(self.coef_)
        sigma = np.sqrt(X_ast.dot(self.sigma_).dot(X_ast).T)
        if return_std:
            return mu, np.diag(sigma.reshape(n_ast, n_ast))
        elif return_cov:
            return mu, sigma
        else:
            return mu

    def compute_lower_bound(self):
        # Computes evidence lower bound
        elbo = (
                -self.n_train / 2 * np.log(2 * np.pi)
                + self.hyper.c_0 / 2 * np.log(self.hyper.d_0 / 2)
                - gammaln(self.hyper.c_0 / 2)
                - self.var.c_ast * np.log(self.var.d_ast)
                + gammaln(self.var.c_ast)
                + self.var.lambda_.T.dot(self.var.tau)
                + self.var.phi * np.sum(self.var.lambda_)
                - self.p * (gammaln(self.hyper.a) + gammaln(self.hyper.b))
                - 2 * gammaln(1 / 2)
                + np.log(2) * ((5/4 - self.hyper.a / 2) * self.p - 1 / 2)
                + self.p * gammaln(self.hyper.a + self.hyper.b)
                + gammaln(self.p * self.hyper.b + 1 / 2)
                + 1 / 2 * self.var.omega
                + self.var.phi * self.var.omega
                + 1 / 2 * np.linalg.slogdet(self.var.Sigma)[1]
                - (self.hyper.a / 2 - 1 / 4) * (
                        np.sum(np.log(self.var.lambda_)) - self.p * np.log(self.var.sigma2_inv))
                + (self.hyper.a / 2 - 1 / 4) * np.sum(np.log(np.diag(self.var.bb).reshape(-1, 1)))
                + np.sum(np.log(kv(self.hyper.a - 1 / 2, self.var.v)))
                - self.var.sigma2_inv / 2 * (
                    np.diag(self.var.bb).reshape(-1, 1).T.dot(self.var.tau_inv))
                - (self.p * self.hyper.b + 1 / 2) * np.log(self.var.omega + np.sum(self.var.lambda_))
                - (self.hyper.a + self.hyper.b) * np.sum(np.log(self.var.tau + self.var.phi))
                - np.log(self.var.phi + 1)
        )
        return elbo

    def preprocess_data(self,X,y):
        #get data in correct shape and save it
        # Singleton training set_with_var reshaping
        if np.shape(np.shape(X))[0] == 1:
            self.n_train = 1
            self.p = np.size(X)
            self.X_train = np.reshape(X, (1, -1))
        else:
            (self.n_train, self.p) = np.shape(X)
            self.X_train = X

        self.y_train = y.reshape(-1, 1)

    # def get_params(self):
    #     test = 1
    #
    # def sample_y(self,X,n_samples=1):
    #     test = 1


class HyperParam(object):
    # Class for the hyper parameters
    def __init__(self, a, b, c_0, d_0):
        self.a = a
        self.b = b
        self.c_0 = c_0
        self.d_0 = d_0


class VariationalParam(object):
    #Class for the variational parameters
    def __init__(self, p, hyper):
        self.mu = np.zeros(p).reshape(-1, 1)
        self.Sigma = np.eye(p)
        self.Sigma_inv = np.eye(p)
        self.bb = self.Sigma + self.mu.dot(self.mu.T)
        self.c_ast = (0 + p + hyper.c_0) / 2
        self.d_ast = np.random.exponential(1)
        self.sigma2_inv = self.c_ast / self.d_ast

        self.lambda_ = np.ones((p, 1)) * np.random.exponential(1)
        self.phi = np.random.exponential(1)
        self.omega = np.random.exponential(1)

        self.v = np.sqrt(2 * self.lambda_ * self.sigma2_inv * np.diag(self.bb).reshape(-1, 1))
        self.tau = np.ones((p, 1)) * np.random.exponential(1)
        self.tau_inv = np.ones((p, 1)) * np.random.exponential(1)

        self.T_inv = np.diag(np.squeeze(self.tau_inv))
        self.A_inv = np.diag(1 / np.squeeze(self.tau_inv))

    def set_with_nat(self, nat):
        # set the variational parameters (var) using the natural parameters (nat)
        eta_beta_2_inv = inv(nat.beta[1])

        self.mu = -1 / 2 * eta_beta_2_inv.dot(nat.beta[0])
        self.Sigma = -1 / 2 * eta_beta_2_inv
        self.Sigma_inv = -2 * nat.beta[1]
        self.bb = self.Sigma + self.mu.dot(self.mu.T)
        self.c_ast = nat.sigma_inv[0] + 1
        self.d_ast = -nat.sigma_inv[1]
        self.sigma2_inv = self.c_ast / self.d_ast
        self.v = np.sqrt(-2 * nat.tau[1] * 2 * nat.tau[2])
        self.tau = np.sqrt(-nat.tau[2] / nat.tau[1]) * kv(nat.tau[0] + 2, self.v) / kv(nat.tau[0] + 1, self.v)
        self.tau_inv = np.sqrt(-nat.tau[1] / nat.tau[2]) * kv(-nat.tau[0], self.v) / kv(-nat.tau[0] - 1, self.v)
        self.lambda_ = (nat.lambda_[0] + 1) / (-1 * nat.lambda_[1])
        self.phi = (nat.phi[0] + 1) / (-1 * nat.phi[1])
        self.omega = (nat.omega[0] + 1) / (-1 * nat.omega[1])
        self.A_inv = self.sigma2_inv * self.Sigma
        self.T_inv = np.diag(np.squeeze(self.tau_inv))

    def get_nat_param(self, p, hyper):
        # return the natural parameters (nat) based on the variational parameters (var)
        nat = VariationalNatParam(p, hyper)
        nat.beta = [self.Sigma_inv.dot(self.mu), -1 / 2 * self.Sigma_inv]
        nat.sigma_inv = [self.c_ast - 1, -self.d_ast]
        nat.tau = [hyper.a - 3 / 2, -self.lambda_, np.diag(self.bb).reshape(-1, 1).dot(self.sigma2_inv) / 2]
        nat.lambda_ = [hyper.a + hyper.b - 1, -self.tau - self.phi]
        nat.phi = [p * hyper.b - 1 / 2, -np.sum(self.lambda_) - self.omega]
        nat.omega = [0, -self.phi - 1]

        return nat


class VariationalNatParam(object):
    # Class for the natural parameters
    def __init__(self, p, hyper):
        self.beta = [np.zeros((p, 1)), np.zeros((p, p))]
        self.sigma_inv = [hyper.c_0 / 2 - 1, -hyper.d_0 / 2]
        self.tau = [hyper.a - 3 / 2, np.zeros((p, 1)), np.zeros((p, 1))]
        self.lambda_ = [hyper.a + hyper.b - 1, np.zeros((p, 1))]
        self.phi = [p * hyper.b - 1 / 2, 0]
        self.omega = [0, 0]

    def set_with_var(self, p, hyper, var):
        # set the the natural parameters (nat) using variational parameters (var)
        self.beta = [var.Sigma_inv.dot(var.mu), -1 / 2 * var.Sigma_inv]
        self.sigma_inv = [var.c_ast - 1, -var.d_ast]
        self.tau = [hyper.a - 3 / 2, -var.lambda_, np.diag(var.bb).reshape(-1, 1).dot(var.sigma2_inv) / 2]
        self.lambda_ = [hyper.a + hyper.b - 1, -var.tau - var.phi]
        self.phi = [p * hyper.b - 1 / 2, -np.sum(var.lambda_) - var.omega]
        self.omega = [0, -var.phi - 1]

    def get_normal_param(self, p, hyper):
        # return the variational parameters (var) based on the natural parameters (nat)
        eta_beta_2_inv = inv(self.beta[1])

        var = VariationalParam(p, hyper)
        var.mu = -1 / 2 * eta_beta_2_inv.dot(self.beta[0])
        var.Sigma = -1 / 2 * eta_beta_2_inv
        var.Sigma_inv = -2 * self.beta[1]
        var.bb = var.Sigma + var.mu.dot(var.mu.T)
        var.c_ast = self.sigma_inv[0] + 1
        var.d_ast = -self.sigma_inv[1]
        var.sigma2_inv = var.c_ast / var.d_ast
        var.v = np.sqrt(-2 * self.tau[1] * 2 * self.tau[2])
        var.tau = np.sqrt(-self.tau[2] / self.tau[1]) * kv(self.tau[0] + 2, var.v) / kv(self.tau[0] + 1, var.v)
        var.tau_inv = np.sqrt(-self.tau[1] / self.tau[2]) * kv(-self.tau[0], var.v) / kv(-self.tau[0] - 1, var.v)
        var.lambda_ = (self.lambda_[0] + 1) / (-1 * self.lambda_[1])
        var.phi = (self.phi[0] + 1) / (-1 * self.phi[1])
        var.omega = (self.omega[0] + 1) / (-1 * self.omega[1])
        var.A_inv = var.sigma2_inv * var.Sigma
        var.T_inv = np.diag(np.squeeze(var.tau_inv))

        return var

    def gradient_update(self, nat_hat, rho_t):
        # do a gradient update based on the intermediate natural parameters nat_hat with step size rho_t
        self.beta = [(1 - rho_t) * self.beta[param_idx] + rho_t * nat_hat.beta[param_idx] for param_idx in range(2)]
        self.sigma_inv = [(1 - rho_t) * self.sigma_inv[param_idx] + rho_t * nat_hat.sigma_inv[param_idx] for param_idx
                          in range(2)]
        self.tau = [(1 - rho_t) * self.tau[param_idx] + rho_t * nat_hat.tau[param_idx] for param_idx in range(3)]
        self.lambda_ = [(1 - rho_t) * self.lambda_[param_idx] + rho_t * nat_hat.lambda_[param_idx] for param_idx in
                        range(2)]
        self.phi = [(1 - rho_t) * self.phi[param_idx] + rho_t * nat_hat.phi[param_idx] for param_idx in range(2)]
        self.omega = [(1 - rho_t) * self.omega[param_idx] + rho_t * nat_hat.omega[param_idx] for param_idx in range(2)]


# TODO This class is unnecessary it can be integrated into variational nat_param
class NatEstimate(object):
    #class for the intermediate natural parameters
    def __init__(self, nat):
        self.beta = [nat.beta[0], nat.beta[1]]
        self.sigma_inv = ([- nat.sigma_inv[0], -nat.sigma_inv[1]])
        self.tau = [- nat.tau[0], - nat.tau[1]]
        self.lambda_ = [- nat.lambda_[0], - nat.lambda_[1]]
        self.phi = [- nat.phi[0], - nat.phi[1]]
        self.omega = [0, - nat.omega[1]]

    def estimate_nat(self, hyper, var, X_sam, X_T_X_sam, y_sam, N, p):
        # estimates the intermediate natural parameters using the data X,y and N replicates
        self.beta = [var.sigma2_inv * N * X_sam.T.dot(y_sam),
                     -1 / 2 * var.sigma2_inv * (N * X_T_X_sam + var.T_inv)]

        self.sigma_inv = ([(N + p + hyper.c_0) / 2 - 1,
                           -(N * y_sam.T.dot(y_sam)
                             - 2 * N * y_sam.T.dot(X_sam).dot(var.mu)
                             + N * np.sum(var.bb * X_T_X_sam.T)
                             + np.diag(var.bb).reshape(-1, 1).T.dot(
                                       var.tau_inv) + hyper.d_0) / 2])

        self.tau = [hyper.a - 3 / 2, -var.lambda_,
                    np.diag(var.bb).reshape(-1, 1).dot(var.sigma2_inv) / 2]

        self.lambda_ = [hyper.a + hyper.b - 1,
                        - var.tau - var.phi]
        self.phi = [p * hyper.b - 1 / 2,
                    -np.sum(var.lambda_) - var.omega]
        self.omega = [0, -var.phi - 1]
