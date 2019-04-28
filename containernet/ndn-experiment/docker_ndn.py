#!/usr/bin/python

from mininet.net import Containernet
from mininet.node import Docker, OVSSwitch
from mininet.log import setLogLevel, info
from mininet.link import TCLink
from mininet.topo import Topo
from mininet.util import dumpNodeConnections
import time
from lxml import etree
from subprocess import call
from threading import Thread
import os
import argparse
import locks

SEGMENT_LENGTH = 2
NFD_STRATEGY = 'access'

setLogLevel('info')

class OVSBridgeSTP( OVSSwitch ):
    """Open vSwitch Ethernet bridge with Spanning Tree Protocol
       rooted at the first bridge that is created."""
    prio = 4096
    def start( self, *args, **kwargs ):
        OVSSwitch.start( self, *args, **kwargs )
        self.cmd( 'ovs-vsctl set-fail-mode', self, 'standalone' )
        self.cmd( 'ovs-vsctl set-controller', self )
        self.cmd( 'ovs-vsctl set Bridge', self,
                  'rstp_enable=true',
                  'mcast_snooping_enable=true',
                  'other_config:rstp-priority=%d' % OVSBridgeSTP.prio )

#switches = { 'ovs-stp': OVSBridgeSTP } TODO remove

def setNDNNodeInfo(mn_topo, node, nodeType, segments=15):
    """Specify parameters for client, server, and cache nodes.
       s: segments, t: type, v: videos folder, r: repo_ng options, 
       c: cache size"""
    if 'server' in nodeType:
        mn_topo.setNodeInfo(node, {
            "dcmd": ["/bin/bash", "-c",
                     "/ndn-entrypoint.sh -v /videos/ -t repong-bbb -s {0} -c 0 -r '-D -u'"
                     .format(segments)],  
            "dimage": "ndn_repo_ndn:latest",
            "privileged": True,
            "publish_all_ports": True,
            "cls": Docker
        })
    elif 'client' in nodeType:
        mn_topo.setNodeInfo(node, {
                "dcmd": ["/bin/bash", "/ndn-entrypoint.sh"],
                "dimage": "ndn_headless-player_ndn-player:latest",
                "privileged": True,
                "publish_all_ports": True,
                "cls": Docker
        })
    elif 'cache' in nodeType:
        #caches have 0 segments and a defined cache size
        mn_topo.setNodeInfo(node, {
                "dcmd": ["/bin/bash", "-c",
                         "/ndn-entrypoint.sh -t repong-bbb -s 0 -v /opt/ -c 15000"],
                "dimage": "ndn_repo_ndn:latest",
                "privileged": True,
                "publish_all_ports": True,
                "cls": Docker
        })

def initExperiment(toptype, duration, algo, qoe_weights, use_memory, epoch):
    """Define the topologies and begin the experiment"""
    info('*** Executing {0} topology'.format(toptype))
    # Set link attributes
    segments = duration/SEGMENT_LENGTH
    delaycore = '10ms'
    delayedge = '20ms'
    delayserver = '40ms'
    bwcore = 1000
    bwedge = 1000
    bwserver = 20
    linkloss = 0
    queue=1000
    # Define topologies
    if 'large-scale' == toptype:
        # Randomly-generated large-scale topology. Note that it was randomly generated with
        # make_topo.py, but this implementation is fixed.
        num_clients = 7
        num_servers = 4
        num_caches = 5
        topology = Topo()
        clients = ['client{}'.format(i+1) for i in range(num_clients)]
        servers = ['server{}'.format(i+1) for i in range(num_servers)]
        caches = ['cache{}'.format(i+1) for i in range(num_caches)]
        switches = ['switch{}'.format(i+1) for i in range(num_caches)]
        for sw in switches:
            topology.addSwitch(sw, cls=OVSBridgeSTP)
        for se in servers:
            host = topology.addHost(se)
            setNDNNodeInfo(topology, host, 'server', segments=str(segments))
        for ca in caches:
            host = topology.addHost(ca)
            setNDNNodeInfo(topology, host, 'cache')
        for cl in clients:
            host = topology.addHost(cl)
            setNDNNodeInfo(topology, host, 'client')
        # Link nodes. Clients, servers, and caches are only linked to one node, so the idx in the list
        # corresponds to the idx in `clients`, and the number is the switch idx to attach to. Same for
        # cache and server.
        client_links = [3, 3, 4, 4, 2, 3, 4]
        server_links = [int(i/2) for i in range(num_servers)]
        cache_links = list(range(num_caches))
        # Caches are connected to multiple other caches, so use the same idea as above, but it's now
        # a list of lists. Note that each connection is only listed once (e.g. 5->7 is listed, but
        # not 7->5) as mininet assumes all links are bidirectional.
        switch_links = [[2], [2, 3, 4], [3], [4]]
        for ca, link in zip(caches, cache_links):
            topology.addLink(ca, switches[link], cls=TCLink, delay=delaycore, bw=bwcore,
                             loss=linkloss, max_queue_size=queue)
        for se, link in zip(servers, server_links):
            topology.addLink(se, switches[link], cls=TCLink, delay=delayserver, bw=bwserver,
                             loss=linkloss, max_queue_size=queue)
        for cl, link in zip(clients, client_links):
            topology.addLink(cl, switches[link], cls=TCLink, delay=delayedge, bw=bwedge,
                             loss=linkloss, max_queue_size=queue)
        for sw, links in zip(switches, switch_links):
            for link in links:
                topology.addLink(sw, switches[link], cls=TCLink, delay=delaycore, bw=bwcore,
                                 loss=linkloss, max_queue_size=queue)
    if 'doubles' == toptype:
        # A topology with 2 clients, 2 caches, and 2 servers. Both clients are
        # connected to one cache, and both servers are connected to the other.
        # The two caches are connected, connecting the whole network.
        topology = Topo()
        caches = ['cache1', 'cache2']
        clients = ['client1', 'client2']
        servers = ['server1', 'server2']
        switches = ['switch1', 'switch2']
        for sw in switches:
            topology.addSwitch(sw, cls=OVSBridgeSTP)
        for se in servers:
            host = topology.addHost(se)
            setNDNNodeInfo(topology, host, 'server', segments=str(segments))
        for ca in caches:
            host = topology.addHost(ca)
            setNDNNodeInfo(topology, host, 'cache')
        for cl in clients:
            host = topology.addHost(cl)
            setNDNNodeInfo(topology, host, 'client')
        # Link nodes
        topology.addLink(caches[0], switches[0], cls=TCLink, delay=delaycore,
                         bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(caches[1], switches[1], cls=TCLink, delay=delaycore,
                         bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(servers[0], switches[1], cls=TCLink, delay=delayserver,
                         bw=bwserver, loss=linkloss, max_queue_size=queue)
        topology.addLink(servers[1], switches[1], cls=TCLink, delay=delayserver,
                         bw=bwserver, loss=linkloss, max_queue_size=queue)
        topology.addLink(switches[0], switches[1], cls=TCLink, delay=delaycore,
                         bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(clients[0], switches[0], cls=TCLink, delay=delayedge,
                         bw=bwedge, loss=linkloss, max_queue_size=queue)
        topology.addLink(clients[1], switches[0], cls=TCLink, delay=delayedge,
                         bw=bwedge, loss=linkloss, max_queue_size=queue)

    # Create the network and perform additional setup
    global net
    net = Containernet(topo=topology)
    for h in net.hosts:
        setStrategy(h, '/n', NFD_STRATEGY)
    info('*** Network connections:\n')
    dumpNodeConnections(net.hosts)
    dumpNodeConnections(net.switches)
    info('*** Starting network\n')
    net.start()
    info('\n*** Adding NDN links:\n')
    for client in clients:
        for cache in caches:
            addNDNRoute(client, cache, con_type='udp', cost=2)
    for server in servers:
        for cache in caches:
            addNDNRoute(cache, server, con_type='udp', cost=10)
    for cache1 in caches:
        for cache2 in caches:
            if cache1 != cache2:
                addNDNRoute(cache1, cache2, con_type='udp', cost=1)

    # Wait until all clients have finished
    info('*** Waiting for clients to finish playback\n')
    waitForPlayback(servers, clients, switches, net, duration, algo,
                    qoe_weights, use_memory, epoch, toptype)

    # Teardown
    info('*** Stopping network\n')
    net.stop()

def waitForPlayback(servers, clients, switches, net, playbacktime, algo,
                    qoe_weights, use_memory, epoch, toptype):
    # Wait for repo-ng to initialize
    while not (checkRepoNGInitDone(net.get(servers[0]))):
        info('*** Waiting for repo-ng to initialize database...\n')
        time.sleep(5)
    
    info('*** Starting clients')
    locks.STOP_TC = False
    # Sculpt traffic for doubles topology
    if toptype == 'doubles':
        tc_switch0 = Thread(target=traffic_control,
                            args = (net.get(switches[0]), # FROM here...
                                    net.get(switches[1]), # ...TO here.
                                    epoch))
        tc_switch1 = Thread(target=traffic_control,
                            args = (net.get(switches[1]),
                                    net.get(switches[0]),
                                    epoch))
        tc_switch0.start()
        tc_switch1.start()

    # Begin streaming
    for client in clients:
        client_thread = Thread(target=startPlayer, kwargs = {
            'host': net.get(client),
            'aa': algo,
            'segmentsizeOn': False,
            'sampleSize': 50,
            'client': client,
            'qoe_weights': qoe_weights,
            'use_memory': use_memory,
            'epoch': epoch,
            'timesteps_per_epoch': int(playbacktime/2)
        })
        client_thread.start()

    # Wait until streaming finished, giving some extra time due to stalling,
    # then stop the client and get its data
    time.sleep(playbacktime + 20)
    for client in clients:
        killPlayer(net.get(client))
    locks.STOP_TC = True
    for client in clients:
        getDataFromClient(algo, toptype, playbacktime, qoe_weights, use_memory, client, epoch)

def getDataFromClient(algo, topology, duration, qoe_weights, use_memory,
                      client, epoch):
    """Fetch results and UCB object from client containers"""
    # Determine the filename based on the client and playback parameters
    weights_str = '-'.join([str(q) for q in qoe_weights])
    memory_str = 'memory' if use_memory else 'no-memory'
    filename = "../results/{0}_{1}_{2}_{3}_{4}_{5}_{6}".format(algo, topology,
               duration, weights_str, memory_str, client, epoch)
    # Fetch the results
    try:
        os.makedirs(os.path.dirname(filename))
    except:
        pass
    with open(filename, "w") as text_file:
        text_file.write(net.get(client).cmd('cat /player-out.txt'))
    # Get the UCB object if we'll need it for the next epoch
    if use_memory:
        weights_str = '_'.join([str(int(q)) for q in qoe_weights])
        ucb_obj_path = '/home/ubuntu/containernet/ucb_objs/{0}/{1}/{2}_obj.pkl'.format(
            client, weights_str, algo)
        call('docker cp mn.{0}:/{1}.pkl {2}'.format(client, algo, ucb_obj_path),
             shell=True)
    info('*** Copied data to: {0}\n'.format(filename))

def addNDNRoute(source, dest, cost=1, con_type='tcp'):
    source = net.get(source)
    dest = net.get(dest)
    if con_type == 'tcp':
        return source.sendCmd("nfdc register -c {0} /n tcp://{1}".format(
            cost, dest.IP()))
    elif con_type == 'udp':
        return source.sendCmd("nfdc register -c {0} /n udp://{1}".format(
            cost, dest.IP()))
    elif con_type == 'ether':
        source.cmdPrint('sysctl -w net.ipv4.ip_forward=1')
        source.cmdPrint(
            'ip link add name server link {}-eth0 type macvlan'.format(source.name))
        source.cmdPrint(
            'ip link set dev server address {}'.format(dest.MAC()))
        source.cmdPrint('ip link set server up')
        source.cmdPrint(
            'ip addr add {}/32 brd + dev server'.format(dest.IP()))
        source.cmdPrint('ip route add {} dev server'.format(source.IP()))
        source.cmdPrint('ifconfig')
        time.sleep(2)
        xml = source.cmd('nfd-status -x')
        doc = etree.fromstring(xml)
        face_ID = 'Not Set'
        for b in doc.findall(".//{ndn:/localhost/nfd/status/1}localUri/."):
            if "server" in b.text:
                print 'Found Face ID'
                face_ID = b.getparent().getchildren()[0].text
                print face_ID
        return source.cmdPrint("nfdc register -c {0} /n {1}".format(cost, face_ID))
    elif con_type == 'auto':
        source.sendCmd('nfd-autoreg --prefix=/n -w 10.0.0.0/8 &')

def checkRepoNGInitDone(node):
    status = node.cmd('ls /INITDONE')
    if 'No such file or directory' not in status:
        return True
    else:
        return False

def setStrategy(node, name, strategy):
    print node.cmd("nfdc set-strategy %s ndn:/localhost/nfd/strategy/%s" %
                   (name, strategy))

def startPlayer(host=None, mpd="/n/mpd", aa='p', alpha='44', segmentsizeOn=True,
                sampleSize='', client='N/A', qoe_weights=None, use_memory=False,
                epoch=-1, timesteps_per_epoch=100):
    """Begin playback.
       /code/executeDockerScenario.sh -nohead -n -bola 0.8 12 -u /n/mpd
       p is extended PANDA, alpha 
       bola is bola, alpha is 12
       fb is regular panda
       br BufferRateBased (AdapTech)
       r ratebased
       b BufferBased regular
       bt BufferBasedThreeThreshold  
       nr no adaptation (test for caching)
    """
    # TODO Player does not start
    if aa == 'bola' or aa == 'fb':
        sampleSize = ''
    if sampleSize != '':
        sampleSize = '-c {}'.format(sampleSize) 
    localmpd = ''
    if segmentsizeOn:
        localmpd = '-localmpd /code/mpdss'
    PLAYER_PATH = "/code/executeDockerScenario.sh -nohead"
    ADAPDATION_ALGORITHM = "{0} -n 0.8 {1} -{2} {3} {4} {5} {6}".format(localmpd, sampleSize, aa, alpha, *qoe_weights)
    MPD_COMMAND = "-u {0} > /player-out.txt &".format(mpd)

    # If we use memory, upload any existing objects
    # epoch > 0 to overwrite previous memory objects once this epoch finishes
    if use_memory and epoch > 0:
        ucb_obj_folder = '/home/ubuntu/containernet/ucb_objs/{0}/{1}/'.format(
            client, '_'.join([str(int(q)) for q in qoe_weights]))
        ucb_obj_path = ucb_obj_folder + '{0}_obj.pkl'.format(aa)
        if not os.path.exists(ucb_obj_folder):
            print("ERROR: ucb_objs/client/weights folder does not exist!")
            exit(1)
        if os.path.exists(ucb_obj_path):
            call('docker cp {0} mn.{1}:/{2}.pkl'.format(ucb_obj_path, client, aa), shell=True)
            host.cmd('export BEGIN_TIMESTEP="{0}"'.format((epoch-1)*timesteps_per_epoch))
        elif epoch > 1:
            print('WARNING: no UCB object found while using memory with epoch > 1')

    # Begin playback
    host.cmd('rm log')
    host.cmd(['chmod +x', '/code/executeDockerScenario.sh'])
    host.cmd('ndnpeek /n/mpd')
    host.sendCmd([PLAYER_PATH, ADAPDATION_ALGORITHM, MPD_COMMAND])

def killPlayer(host):
    print host.cmd('pkill qtsampleplayer')

if __name__ == '__main__':
    setLogLevel('info')
    # Parse the arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-a', '--algo', nargs=1, type=str, required=True)
    parser.add_argument('-t', '--topology', nargs=1, type=str, required=True)
    parser.add_argument('-d', '--duration', nargs=1, type=int, required=True)
    parser.add_argument('-q', '--qoe_weights', nargs=3, type=float, required=True)
    parser.add_argument('--memory', dest='use_memory', action='store_true')
    parser.add_argument('--no-memory', dest='use_memory', action='store_false')
    parser.set_defaults(use_memory=False)
    parser.add_argument('-e', '--epoch', nargs=1, type=int, required=True)
    args = vars(parser.parse_args())
    initExperiment(toptype=args['topology'][0], duration=args['duration'][0],
                   algo=args['algo'][0], qoe_weights=args['qoe_weights'], 
                   use_memory=args['use_memory'], epoch=args['epoch'][0])
