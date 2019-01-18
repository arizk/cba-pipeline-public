# Bandits

This module includes the Bandit algorithm using the contextual Bayes UCB with continous shrinkage prior and one step stochastic variational inference (SVI).  Several other Bandit implementations known from the literature are implemented as well. 
The module GbRegression is an implementation of the Three Parameter Beta Normal prior. Additionally, an implemtation of the stochastic approximation for SVI is provided

The related algorithms are my interpretation, please double check the implementations.


# Demo
Several example applications can be found in the folder 'test'.

# References
[1] Armagan, Artin, Merlise Clyde, and David B. Dunson. "Generalized beta mixtures of Gaussians." Advances in neural information processing systems. 2011. (http://papers.nips.cc/paper/4439-generalized-beta-mixtures-of-gaussians)

[2] Kaufmann, Emilie, Olivier Cappé, and Aurélien Garivier. "On Bayesian upper confidence bounds for bandit problems." Artificial Intelligence and Statistics. 2012. (http://proceedings.mlr.press/v22/kaufmann12/kaufmann12.pdf)

[3] Krause, Andreas, and Cheng S. Ong. "Contextual gaussian process bandit optimization." Advances in Neural Information Processing Systems. 2011.(https://papers.nips.cc/paper/4487-contextual-gaussian-process-bandit-optimization)

[4] Li, Lihong, et al. "A contextual-bandit approach to personalized news article recommendation." Proceedings of the 19th international conference on World wide web. ACM, 2010. (https://arxiv.org/pdf/1003.0146.pdf)

[5] Bastani, Hamsa, and Mohsen Bayati. "Online decision-making with high-dimensional covariates." (2015). (http://web.stanford.edu/~bayati/papers/LassoBandit.pdf)

