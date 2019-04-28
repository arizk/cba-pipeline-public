import time
import re
import pandas as pd
import re
from dynamicLinkChange import *
import locks

TRACE = 'client1' # TODO remove, simply due to trace filenames

def traffic_control(to_host, switch, epoch=-1):
    print('=======================================')
    print('Starting trace playback')
    print(epoch)
    with open('bandwidth_traces/{}_epoch{}.csv'.format(TRACE, epoch), 'r') as f:
        lines = [line.split(',') for line in f]
        for bw, bw_period in lines:
            if locks.STOP_TC:
                break
            # Double bandwidth because we have 2 clients (assuming doubles topo)
            print(time.time())
            reconfigureConnection(to_host, switch, float(bw))
            time.sleep(float(bw_period))
    print('Done playing bandwidth trace.')
    print('=======================================')
    return()
