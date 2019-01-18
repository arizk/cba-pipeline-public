from graph_tool.all import *
import numpy as np
import math

NUM_CLIENTS = 7
NUM_CACHES = 5
NUM_SERVERS = 4
NUM_INTERNAL_CACHES = 0

# Create the graph
g = Graph(directed=False)
clients = [g.add_vertex() for _ in range(NUM_CLIENTS)]
caches = [g.add_vertex() for _ in range(NUM_CACHES)]
servers = [g.add_vertex() for _ in range(NUM_SERVERS)]
node_type = g.new_vertex_property('int')
type_idx = g.new_vertex_property('int')

# Randomly attach caches to each other
for i, ca1 in enumerate(caches):
    node_type[ca1] = 2
    type_idx[ca1] = i
    for j, ca2 in enumerate(caches[i+1:]):
        if np.random.uniform(low=0, high=1) > 0.5:
            g.add_edge(ca1, ca2)
            print('cache {} connected to cache {}'.format(i, j+i+1))
# Attach each server to a cache (1-to-1)
for i, s in enumerate(servers):
    g.add_edge(s, caches[math.floor(i/2)])
    node_type[s] = 0
    type_idx[s] = i
    print('server {} connected to cache {}'.format(i, math.floor(i/2)))
# Attach each client to a random cache that does NOT have a server attached to it
# Also limit ourselves so there are a few internal caches
for i, cl in enumerate(clients):
    cache = math.floor(np.random.uniform(low=math.floor(NUM_SERVERS/2), high=NUM_CACHES-NUM_INTERNAL_CACHES))
    g.add_edge(cl, caches[cache])
    node_type[cl] = 1
    type_idx[cl] = i
    print('client {} connected to cache {}'.format(i, cache))

def test(v_idx):
    return 'su' + str(v_idx)

# Display the result
graph_draw(g, vertex_text=type_idx, vertex_font_size=10,
           output_size=(500,500), vertex_color=[1,1,1,0],
           vertex_fill_color=node_type, output="ndn-topo.png",
           edge_end_marker='circle', edge_start_marker='circle', 
           edge_pen_width=2)
