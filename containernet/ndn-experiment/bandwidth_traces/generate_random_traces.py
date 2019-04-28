# Note, skip epoch 1 because that one used trevor_trace
import csv
import numpy as np

for i in range(2, 21):
    np.random.seed(i)
    bws = [np.random.normal(7, 3) for _ in range(10000)]
    bws = [bw if bw > 0.5 else 0.5 for bw in bws]
    periods = [np.random.normal(5) for _ in range(10000)]
    periods = [per if per >0.5 else 0.5 for per in periods]
    with open('trace_epoch_{}.csv'.format(i), 'w') as f:
        writer = csv.writer(f)
        writer.writerows(zip(bws, periods))
