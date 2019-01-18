import numpy as np
import csv

for client in range(1, 2):
    for epoch in range(1, 6):
        #bws = [np.random.gamma(6, 0.5) for _ in range(1000)]
        #pds = [np.random.gamma(2, 0.5) for _ in range(1000)]
        bws = [3.7, 3.2, 2.3, 1.7, 1.2, 1.2, 1.7, 2.3, 3.2, 3.7]
        bws = [bw - (epoch-1) * 0.1 for bw in bws]
        pds = [20 for i in range(10)]
        # Add 20 extra seconds to the end since we let it go for 220 seconds total, in case of stalling
        pds[-1] += 20
        lines = zip(bws, pds)
        with open('client{}_epoch{}.csv'.format(client, epoch), 'w') as f:
            writer = csv.writer(f)
            for line in lines:
                writer.writerow(line)
