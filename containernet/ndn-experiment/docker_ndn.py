#!/usr/bin/python
"""

"""
from tabulate import tabulate
from mininet.net import Containernet
from mininet.node import Controller, Docker, OVSSwitch, Switch, OVSBridge, UserSwitch
from mininet.cli import CLI
from mininet.log import setLogLevel, info
from mininet.link import TCLink, Link, OVSLink
from mininet.util import dumpNodeConnections
from mininet.topo import Topo
from time import gmtime, strftime
from trafficControl import *

import fnss
import time
from lxml import etree
import re
#from IPython import embed
import topology as NDNtopo
import random
from ifparser import Ifcfg
from tabulate import tabulate
from mininet.log import info, setLogLevel
import numpy as np
from subprocess import call
from threading import Thread
import os
import argparse

import locks

setLogLevel('info')
'''
netstat -anu|sort -nk4
'''
class OVSBridgeSTP( OVSSwitch ):
    """Open vSwitch Ethernet bridge with Spanning Tree Protocol
       rooted at the first bridge that is created"""
    prio = 4096
    def start( self, *args, **kwargs ):
        OVSSwitch.start( self, *args, **kwargs )
        OVSBridgeSTP.prio *= 1
        self.cmd( 'ovs-vsctl set-fail-mode', self, 'standalone' )
        self.cmd( 'ovs-vsctl set-controller', self )
        self.cmd( 'ovs-vsctl set Bridge', self,
                  'rstp_enable=true',
                  'mcast_snooping_enable=true',
                  'other_config:rstp-priority=%d' % OVSBridgeSTP.prio )

switches = { 'ovs-stp': OVSBridgeSTP }

class NDNContainernet(Containernet):
    """
    A Mininet with Docker related methods.
    Inherits Mininet.
    This class is not more than API beautification.
    """

    def __init__(self, **params):
        # call original Mininet.__init__
        Containernet.__init__(self, **params)

    def buildFromTopo(self, topo=None):
        """Build mininet from a topology object
           At the end of this function, everything should be connected
           and up."""

        # Possibly we should clean up here and/or validate
        # the topo
        if self.cleanup:
            pass

        info('*** Creating network\n')

        if not self.controllers and self.controller:
            # Add a default controller
            info('*** Adding controller\n')
            classes = self.controller
            if not isinstance(classes, list):
                classes = [classes]
            for i, cls in enumerate(classes):
                # Allow Controller objects because nobody understands partial()
                if isinstance(cls, Controller):
                    self.addController(cls)
                else:
                    self.addController('c%d' % i, cls)

        info('*** Adding docker hosts:\n')
        for hostName in topo.hosts():
            self.addDocker(hostName, **topo.nodeInfo(hostName))
            info(hostName + ' ')
            time.sleep(1)

        info('\n*** Adding switches:\n')
        for switchName in topo.switches():
            # A bit ugly: add batch parameter if appropriate
            params = topo.nodeInfo(switchName)
            cls = params.get('cls', self.switch)
            #if hasattr( cls, 'batchStartup' ):
            #    params.setdefault( 'batch', True )
            self.addSwitch(switchName, cls=OVSSwitch, **params) #c,
            info(switchName + ' ')

        info('\n*** Adding links:\n')
        for srcName, dstName, params in topo.links(sort=True, withInfo=True):
            print srcName, dstName, params
            self.addLink(**params) 
            # if srcName in topo.hosts() and dstName in topo.hosts():
            # addNDNRoute(net.get(srcName), net.get(dstName))
            # TODO Add nen routes
            info('(%s, %s) ' % (srcName, dstName))

        info('\n')
'''
s SEGMENTS
t TYPE
v VIDEOS_FOLDER
r REPO_NG_OPTIONS
c CACHESIZE
'''
def setNDNNodeInfo(mn_topo, node, infttype, segments=15):
    if 'server' in infttype:
        info('*** Adding {0} segments\n'.format(segments))
        mn_topo.setNodeInfo(node, {
            "dcmd": ["/bin/bash", "-c", "/ndn-entrypoint.sh -v /videos/ -t repong-bbb -s {0} -c 0 -r '-D -u'".format(segments)],  
            "dimage": "ndn_repo_ndn:latest",
            "privileged": True,
            "publish_all_ports": True,
            "cls": Docker
        })
    elif 'client' in infttype:
        mn_topo.setNodeInfo(node, {
                "dcmd": ["/bin/bash", "/ndn-entrypoint.sh"],
                "dimage": "ndn_headless-player_ndn-player:latest",
                "privileged": True,
                "publish_all_ports": True,
                "cls": Docker
        })
    elif 'cache' in infttype:
        mn_topo.setNodeInfo(node, {
                "dcmd": ["/bin/bash", "-c", "/ndn-entrypoint.sh -t repong-bbb -s 0 -v /opt/ -c 15000"], #caches go 0 segments and a defined cache size
                "dimage": "ndn_repo_ndn:latest",
                "privileged": True,
                "publish_all_ports": True,
                "cls": Docker
        })
           
def initExperiment(toptype='dash-full', duration=20, algo='N/A', qoe_weights=None, use_memory=False, epoch=-1):
    # Set link attributes
    # https://fnss.github.io/doc/core/apidoc/fnss.functions.html

    segment_length = 2
    segments = duration/segment_length
    cache = []
    clients = []
    server = []
    switches = []
    delaycore = '10ms'
    delayedge = '20ms'
    delayserver = '40ms'
    #TODO Delay Server viel hoher
    bwcore = 1000
    bwserver = 20
    bwedge = 1000
    linkloss = 0
    queue=1000
    if 'large-scale' == toptype:
        # Randomly-generated large-scale topology. Note that it was randomly generated with
        # make_topo.py, but this implementation is fixed.
        print('Executing large-scale topology...')
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
        print("Executing doubles topology...")
        topology = Topo()
        caches = ['cache1', 'cache2']
        clients = ['client1', 'client2']
        servers = ['server1', 'server2']
        switches = ['switch1', 'switch2']
        for sw in switches:
            topology.addSwitch(sw, cls=OVSBridgeSTP)
        # TODO: Can we split the content between producers? e.g. high q on 1, low on 2
        for se in servers:
            host = topology.addHost(se)
            setNDNNodeInfo(topology, host, 'server', segments=str(segments)) # <-- segments?
        # Ask denny: "TODO: Reset to be server"?
        for ca in caches:
            host = topology.addHost(ca)
            setNDNNodeInfo(topology, host, 'cache')
        for cl in clients:
            host = topology.addHost(cl)
            setNDNNodeInfo(topology, host, 'client')
        # Link nodes
        topology.addLink(caches[0], switches[0], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(caches[1], switches[1], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(servers[0], switches[1], cls=TCLink, delay=delayserver, bw=bwserver, loss=linkloss, max_queue_size=queue)
        topology.addLink(servers[1], switches[1], cls=TCLink, delay=delayserver, bw=bwserver, loss=linkloss, max_queue_size=queue)
        topology.addLink(switches[0], switches[1], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(clients[0], switches[0], cls=TCLink, delay=delayedge, bw=bwedge, loss=linkloss, max_queue_size=queue)
        topology.addLink(clients[1], switches[0], cls=TCLink, delay=delayedge, bw=bwedge, loss=linkloss, max_queue_size=queue)
    elif 'doubles-1client' == toptype:
        print("Executing doubles with 1 client topology...")
        topology = Topo()
        caches = ['cache1', 'cache2']
        clients = ['client1']
        servers = ['server1', 'server2']
        switches = ['switch1', 'switch2', 'switch3', 'switch4', 'switch5']
        for sw in switches:
            topology.addSwitch(sw, cls=OVSBridgeSTP)
        # TODO: Can we split the content between producers? e.g. high q on 1, low on 2
        for se in servers:
            host = topology.addHost(se)
            setNDNNodeInfo(topology, host, 'server', segments=str(segments)) # <-- segments?
        # Ask denny: "TODO: Reset to be server"?
        for ca in caches:
            host = topology.addHost(ca)
            setNDNNodeInfo(topology, host, 'cache')
        for cl in clients:
            host = topology.addHost(cl)
            setNDNNodeInfo(topology, host, 'client')
        # Link nodes
        topology.addLink(caches[0], switches[1], cls=TCLink, delay=delaycore, bw=20, loss=linkloss, max_queue_size=queue)
        topology.addLink(caches[1], switches[2], cls=TCLink, delay=delaycore, bw=20, loss=linkloss, max_queue_size=queue)
        topology.addLink(servers[0], switches[3], cls=TCLink, delay=delayserver, bw=bwserver, loss=linkloss, max_queue_size=queue)
        topology.addLink(servers[1], switches[4], cls=TCLink, delay=delayserver, bw=bwserver, loss=linkloss, max_queue_size=queue)
        topology.addLink(switches[0], switches[1], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(switches[1], switches[2], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(switches[2], switches[3], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(switches[2], switches[4], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss, max_queue_size=queue)
        topology.addLink(clients[0], switches[0], cls=TCLink, delay=delayedge, bw=bwedge, loss=linkloss, max_queue_size=queue)
    elif 'dash-full' in toptype:
         # Create two hosts.
        topology = Topo()
        caches = ['cache1', 'cache2', 'cache3']
        clients = []
        for i in range(1, 3):
            clients.append("client{0}".format(i))
        servers = ['server1']
        switches = ['switch1', 'switch2', 'switch3', 'switch4']
        for v in switches:
            topology.addSwitch(v, cls=OVSBridgeSTP)#, stp=True, failMode='standalone') #OVSBridgeSTP
        for v in servers:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'server', segments=str(segments))
        for v in caches:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'cache') #TODO Reset to be server
        for v in clients:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'client')

        topology.addLink(caches[0], switches[0], cls=TCLink, delay=delayedge, bw=10, loss=linkloss,  max_queue_size=queue)
        topology.addLink(caches[1], switches[1], cls=TCLink, delay=delayedge, bw=12, loss=linkloss,  max_queue_size=queue)
        topology.addLink(caches[2], switches[2], cls=TCLink, delay=delayedge, bw=14, loss=linkloss,  max_queue_size=queue)
        topology.addLink(servers[0], switches[3], cls=TCLink, delay=delaycore, bw=bwserver, loss=linkloss,  max_queue_size=queue)

        topology.addLink(switches[0], switches[1], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss,  max_queue_size=queue)
        topology.addLink(switches[1], switches[2], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss,  max_queue_size=queue)
        topology.addLink(switches[2], switches[3], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss,  max_queue_size=queue)
        topology.addLink(switches[3], switches[0], cls=TCLink, delay=delaycore, bw=bwcore, loss=linkloss,  max_queue_size=queue)

        #half clients switch one, half switch two
        for client in clients[:len(clients)/2]:
                topology.addLink(client, switches[0], cls=TCLink,  delay=delaycore, bw=bwcore, loss=linkloss,  max_queue_size=queue)
        for client in clients[len(clients)/2:]:
                topology.addLink(client, switches[1], cls=TCLink,  delay=delaycore, bw=bwcore, loss=linkloss,  max_queue_size=queue)
    elif 'dash-small' in toptype:
        # Create two hosts.
        topology = Topo()
        caches = []
        clients = ['client1']
        servers = ['server1']
        switches = ['switch1']
        for v in switches:
            topology.addSwitch(v, cls=OVSBridgeSTP)
        for v in servers:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'server')
        for v in caches:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'cache') #TODO Reset to be server
        for v in clients:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'client')

        topology.addLink(clients[0], switches[0], cls=TCLink, delay=delayedge, bw= bwedge, loss=linkloss,  max_queue_size=queue, use_htb=True)
        topology.addLink(servers[0], switches[0], cls=TCLink, delay=delayedge, bw=bwserver, loss=linkloss, max_queue_size=queue, use_htb=True)
    
    elif 'dash-direct' in toptype:
        # Create two hosts.
        topology = Topo()
        caches = []
        clients = ['client1']
        servers = ['server1']
        switches = []
        for v in switches:
            topology.addSwitch(v, cls=OVSBridgeSTP)
        for v in servers:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'server')
        for v in caches:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'cache') #TODO Reset to be server
        for v in clients:
            host = topology.addHost(v)
            setNDNNodeInfo(topology, host, 'client')

        topology.addLink(clients[0], servers[0], cls=TCLink, delay=delayedge, bw=bwserver, loss=linkloss, max_queue_size=queue)
    
    global net
    net = Containernet(topo=topology ) # , controller=Controller,waitConnected=True, autoSetMacs=True
    # net.addController('c0')
    #i = 10
    #for v in list(set().union(clients)):
    #     net.get(v).updateCpuLimit(cores=str(i%15))
    #     i = i+1

    #ncc, access, best-route2
    strategy = 'access'
    info('\n*** Adding NDN Strategy to : {0}\n'.format(strategy))
    for x in net.hosts:
        setStrategy(x, '/n', strategy) # 'broadcast'= multicast, best-route https://named-data.net/doc/NFD/0.1.0/manpages/nfdc.html

    info('*** Connections network\n')
    dumpNodeConnections(net.hosts)
    dumpNodeConnections(net.switches)
    info('*** Starting network\n')
    net.start()
    

    info('\n*** Adding NDN links:\n')
    #clients know caches
    for client in clients:
        for cache in caches:
            addNDNRoute(client, cache, con_type='udp', cost=2)

    #caches know servers
    for server in servers:
        for cache in caches:
            addNDNRoute(cache, server, con_type='udp', cost=10)

    #caches know caches
    for cachea in caches:
        for cacheb in caches:
            if cachea != cacheb:
                addNDNRoute(cachea, cacheb, con_type='udp', cost=1)
    

    waitForPlayback(servers, clients, switches, net, duration, algo, qoe_weights, use_memory, epoch, toptype)

    info('*** Waiting for clients to finish playback')
    #embed()
    #CLI(net)

    info('*** Stopping network')
    net.stop()

def waitForPlayback(servers, clients, switches, net, playbacktime, algo, qoe_weights, use_memory, epoch, toptype):
    start = time.time()
    while not (checkRepoNGInitDone(net.get(servers[0]))):
        info('*** Waiting for NDN init\n')
        time.sleep(5)
    end = time.time()
    info('*** Init time per video second {0}\n'.format((end - start)/playbacktime))

    info('*** Starting clients')
    #for client in clients:
    aas=[algo for i in range(len(clients))]
    trace_url = "report.2010-09-28_1407CEST.log.txt"
    locks.STOP_TC = False
    # TODO currently only sculpting middle switches to each other
    #tc_thread_switch3 = Thread(target=traffic_control,
    #                           args = (net.get(servers[0]), # doesn't matter
    #                                   net.get(switches[0]), # dev FROM here...
    #                                   net.get(switches[1]), # ...TO here.
    #                                   "url", # doesn't matter
    #                                   trace_url, # doesn't matter
    #                                   250, #doesn't matter
    #                                   playbacktime + 20, # doesn't matter
    #                                   'client1', # doesn't matter
    #                                   epoch))
    #tc_thread_switch4 = Thread(target=traffic_control,
    #                           args = (net.get(servers[0]),
    #                                   net.get(switches[1]),
    #                                   net.get(switches[0]),
    #                                   "url",
    #                                   trace_url,
    #                                   250,
    #                                   playbacktime + 20,
    #                                   'client1',
    #                                   epoch))
    #tc_thread_switch3.start()
    #tc_thread_switch4.start()
    for i, (client, aa) in enumerate(zip(clients, aas)):
        client_thread = Thread(target=startPlayer,
                               kwargs = {'host': net.get(client),
                                         'aa': aa,
                                         'segmentsizeOn': False,
                                         'sampleSize': 50,
                                         'client': client,
                                         'qoe_weights': qoe_weights,
                                         'use_memory': use_memory,
                                         'epoch': epoch,
                                         'timesteps_per_epoch': int(playbacktime/2)})
        client_thread.start()
        ## Stagger by a quarter for each quarter batch
        ## TODO not sure if we want this, since it would mess up our tc visualizations. Not using for now.
        #if ((i+1) % (len(clients)/4) == 0):
        #    time.sleep(playbacktime/4)

    time.sleep(playbacktime + 20)
    #time.sleep((playbacktime*1.75) + 20)

    for client in clients:
        killPlayer(net.get(client))

    locks.STOP_TC = True

    for client, aa in zip(clients, aas):
        getDataFromClient(algo, toptype, playbacktime, qoe_weights, use_memory, client, epoch)

def bandWidthTest(servers, clients, net, playbacktime):
    time.sleep(15)
    server = net.get(servers[0])
    serverIP = server.IP()

    info('*** Starting IPerf on Server\n')
    startIperfServer(server)

    for client in clients:
        result = startIperfClient(net.get(client), serverIP, playbacktime)
        shapeBandwidth(playbacktime, [client])
        info('*** IperfResult {0} {1}\n'.format(client, net.get(client).cmd('cat /iperf')))
        killPlayer(net.get(client))

def getDataFromClient(algo, topology, duration, qoe_weights, use_memory, client, epoch):
    filename = "../results/{0}_{1}_{2}_{3}_{4}_{5}_{6}".format(algo, topology, duration, '-'.join([str(q) for q in qoe_weights]), 
                                                           'memory' if use_memory else 'no-memory', client, epoch)
    try:
        os.makedirs(os.path.dirname(filename))
    except:
        pass
    with open(filename, "w") as text_file:
        text_file.write(net.get(client).cmd('cat /player-out.txt'))
    # Get the ucb object
    if use_memory:
        ucb_obj_path = '/home/ubuntu/containernet/ucb_objs/{0}/{1}/{2}_obj.pkl'.format(client, '_'.join([str(int(q)) for q in qoe_weights]), algo)
        call('docker cp mn.{0}:/{1}.pkl {2}'.format(client, algo, ucb_obj_path), shell=True)
    info('*** Copied data to: {0}\n'.format(filename))

def addNDNRoute(source, dest, cost=1, con_type='tcp'):
    # TODO UDP Does not work, change ti ether
    source = net.get(source)
    dest = net.get(dest)
    if con_type == 'tcp':
        return source.sendCmd("nfdc register -c {0} /n tcp://{1}".format(
            cost, dest.IP()))
    elif con_type == 'udp':
        return source.sendCmd("nfdc register -c {0} /n udp://{1}".format(
            cost, dest.IP()))
    elif con_type == 'ether':
        # TODO: Error cannot find device server
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
        # face_ID = re.findall(
        #    r'\d+', source.cmd('nfd-status | grep {} | cut -d= -f2'.format("server")))
        # print face_ID
        time.sleep(2)
        xml = source.cmd('nfd-status -x')
        doc = etree.fromstring(xml)
        face_ID = 'Not Set'
        for b in doc.findall(".//{ndn:/localhost/nfd/status/1}localUri/."):
            # print b.text
            # print b.getparent().getchildren()[0].text
            if "server" in b.text:
                print 'Found Face ID'
                face_ID = b.getparent().getchildren()[0].text
                print face_ID
        return source.cmdPrint("nfdc register -c {0} /n {1}".format(cost, face_ID))
    elif con_type == 'auto':
        source.sendCmd('nfd-autoreg --prefix=/n -w 10.0.0.0/8 &')


def listFilesInRepo(node):
    return node.cmd('repo-ng-ls')


def getInterfaceStataAllHosts(hosts, ret_data=False):
    data = []
    for host in hosts:
        if_host = Ifcfg(host.cmd('ifconfig -a'))
        for adapter in if_host.interfaces:
            if adapter.find('eth') != -1:
                res = if_host.get_interface(adapter).get_values()
                data.append([host.name, res['interface'], res['rxbytes'],
                             res['txbytes'], res['rxdroppedpkts']])
    # return [x.get_interface(x.interfaces[0]).get_values() for x in ]
    if ret_data:
        return data
    print tabulate(data, headers=['Name', 'Interface', 'RXBytes', 'TXBytes', 'Rxdroppedpkts'])


'''
map(getNDNDataStata, net.hosts)
'''


def getNDNDataStata(hosts, ret_data=False):
    PAKETS = "{ndn:/localhost/nfd/status/1}packetCounters"
    BYTES = "{ndn:/localhost/nfd/status/1}byteCounters"
    INCOMING_BYTES = "{ndn:/localhost/nfd/status/1}incomingBytes"
    OUTGOING_BYTES = "{ndn:/localhost/nfd/status/1}outgoingBytes"
    NDATA = "{ndn:/localhost/nfd/status/1}nDatas"
    NINTEREST = "{ndn:/localhost/nfd/status/1}nInterests"
    INCOMING_PAKETS = "{ndn:/localhost/nfd/status/1}incomingPackets"
    OUTGOING_PAKETS = "{ndn:/localhost/nfd/status/1}outgoingPackets"
    data = []
    for node in hosts:
        xml = node.cmd('nfd-status -x')
        doc = etree.fromstring(xml)
        for b in doc.findall(".//{ndn:/localhost/nfd/status/1}localUri/."):
            # print b.text
            # print b.getparent().getchildren()[0].text
            # TODO SRC, DST different
            if "udp" in b.text:
                ndntype = 'udp'
            elif "tcp" in b.text:
                ndntype = 'tcp'
            elif "ether" in b.text:
                ndntype = 'ether'
            else:
                break
            # print 'Found Face ID'
            els = b.getparent()
            stats = dict([(res.tag, res.text) for res in els.iter()])
            # print stats
            # TODO replace with get
            data.append([node.name, ndntype, stats.get(INCOMING_BYTES, 0), stats.get(OUTGOING_BYTES, 0), stats.get(NDATA, 0),
                         stats.get(NINTEREST, 0),  stats.get(INCOMING_PAKETS, 0),  stats.get(OUTGOING_PAKETS, 0)])
            # if not ret_data:
            # print 'Host: {0}, INCOMING_BYTES: {1}, OUTGOING_BYTES {2}, NDATA {3}, NINTEREST {4}, INCOMING_PAKETS {5}, OUTGOING_PAKETS {6}'.format(node, stats[INCOMING_BYTES], stats[OUTGOING_BYTES], stats[NDATA], stats[NINTEREST],  stats[INCOMING_PAKETS],  stats[OUTGOING_PAKETS])
    if ret_data:
        return data
    print tabulate(data, headers=['Name', 'Type' 'INCOMING_BYTES', 'OUTGOING_BYTES', 'NDATA', 'NINTEREST', 'INCOMING_PAKETS', 'OUTGOING_PAKETS'])


def listLogs(node):
    NFD_LOG = "/var/log/nfd.out"
    NFD_ERROR_LOG = "/var/log/nfd-error.out"

    NDN_COPY_LOG = "/var/log/copy-repo.log"
    NDN_COPY_ERROR = "/var/log/copy-repo-error.log"

    NDNFS_LOG = "/var/log/ndnfs.log"
    NDNFS_SERVER_LOG = "/var/log/ndnfs-server.log"

    REPO_NG_LOG = "/var/log/repong.log"
    REPONG_ERROR_LOG = "/var/log/repong-error.log"

    return node.cmd(
        'tail {0} {1} {2}'.format(REPO_NG_LOG, NFD_LOG, NDN_COPY_LOG))


"""
/ndn/broadcast/ndnfs/videos/DashJsMediaFile/ElephantsDream_H264BPL30_0100.264.dash/%FD%00%00%01%5D%A3O%EF1/%00%192/sha256digest=981f889303b396519c1c1dc01aa472ab4714e70f0a283aba0679b59583fead17
/ndn/broadcast/ndnfs/videos/DashJsMediaFile/ElephantsDream_H264BPL30_0100.264.dash/%FD%00%00%01%5D%A3O%EF1/%00%193/sha256digest=2a4ee843e3ac3b21f341de4ec45b33eaaef49dbb7567edea32ea60f14a44a62d
/ndn/broadcast/ndnfs/videos/DashJsMediaFile/ElephantsDream_H264BPL30_0100.264.dash/%FD%00%00%01%5D%A3O%EF1/%00%194/sha256digest=d30753f707f5a983bbad98b0c37ba6ddc29da0dfb2068eb40b61ad78ee9b06fb
/ndn/broadcast/ndnfs/videos/DashJsMediaFile/ElephantsDream_H264BPL30_0100.264.dash/%FD%00%00%01%5D%A3O%EF1/%00%195/sha256digest=7e17adb27923a9af8677a09e46217a73ac1d58def212073ab09f482dcc6e163c
Total number of data = 72910

For TOS Dataset and Repo-Ng Total data should be
Total number of data = 39541
"""


def checkRepoNGInitDone(node):
    status = node.cmd('ls /INITDONE')
    if 'No such file or directory' not in status:
        return True
    # s = node.cmd('repo-ng-ls')
    # p = re.compile(r'^Total number of data = +(\d+)', re.M)
    # try:
    #     status = re.findall(p, s)[0]
    # except:
    #     status = 0
    # if int(status) == 88460:
    #     return True
    else:
        # repo-ng-ls | grep init | tail -n 1
        # print status
        return False

def listFiles(node):
    print node.cmd("find /videos/")

'''FINISHED DOWNLOADING ndn:/n/a/10'''
def checkPlaybackDone(node, time, segmentlegth=2):
    status = node.cmd('grep "FINISHED DOWNLOADING" /log | tail -n 1')
    #if 'No such file or directory' not in status:
    #    return True
    # s = node.cmd('repo-ng-ls')
    expectsegment = 50
    latestsegment = re. re.findall(r'\d+', status)[0]
    print "Got status {0}, extracted latest segment {1}, expect {2}".format(status, latestsegment, expectsegment)
    if expectsegment == latestsegment:
        return True
    else:
        return False

def listFiles(node):
    print node.cmd("find /videos/")
# https://github.com/signorello/mini-ndn/blob/master/ndn/nfd.py


def setStrategy(node, name, strategy):
    print node.cmd("nfdc set-strategy %s ndn:/localhost/nfd/strategy/%s" %
                   (name, strategy))
    # time.sleep(0.5)


def getNFDInfo(host):
    data = host.cmd('nfdc status report xml')
    # TODO Parse XML


def registerFace(host, faceid, prefix, cost=0):
    print host.cmd("nfdc register -c {0} {1} {2}".format(cost, faceid, prefix))


def getFaces(host):
    return host.cmd('nfd-status -f')


def getRoutes(host):
    return host.cmd('nfd-status -r')


'''/code/executeDockerScenario.sh -nohead -n -bola 0.8 12 -u /n/mpd
p is extended PANDA, alpha 
bola is bola, alpha is 12
fb is regular panda
br BufferRateBased (AdapTech)
r ratebased
b BufferBased regular
bt BufferBasedThreeThreshold  
nr no adaptation (test for caching)
'''

def startPlayer(host=None,
                mpd="/n/mpd",
                aa='p',
                alpha='44',
                segmentsizeOn=True,
                sampleSize='',
                client='N/A',
                qoe_weights=None,
                use_memory=False,
                epoch=-1,
                timesteps_per_epoch=100):
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
    # data = host.cmd([PLAYER_PATH, ADAPDATION_ALGORITHM, MPD_COMMAND])
    print(' '.join(str(x)
                   for x in [PLAYER_PATH, ADAPDATION_ALGORITHM, MPD_COMMAND]))
    data = ""

    # If we use memory, upload any existing objects
    # epoch > 0 to overwrite previous memory objects once this epoch finishes
    if use_memory and epoch > 0:
        ucb_obj_folder = '/home/ubuntu/containernet/ucb_objs/{0}/{1}/'.format(client, '_'.join([str(int(q)) for q in qoe_weights]))
        ucb_obj_path = ucb_obj_folder + '{0}_obj.pkl'.format(aa)
        if not os.path.exists(ucb_obj_folder):
            print("ERROR: ucb_objs/client/weights folder does not exist!")
            exit(1)
        if os.path.exists(ucb_obj_path):
            call('docker cp {0} mn.{1}:/{2}.pkl'.format(ucb_obj_path, client, aa), shell=True)
            # TODO don't use hardcoded num segments
            host.cmd('export BEGIN_TIMESTEP="{0}"'.format((epoch-1)*timesteps_per_epoch))
        elif epoch > 1:
            print('WARNING: no UCB object found while using memory with epoch > 1')

    # TODO Add in Docker directly
    host.cmd('rm log')
    host.cmd(['chmod +x', '/code/executeDockerScenario.sh'])
    host.cmd('ndnpeek /n/mpd')
    host.sendCmd([PLAYER_PATH, ADAPDATION_ALGORITHM, MPD_COMMAND])
    # while (host.waiting):
    #    print host.monitor()

    # data = host.cmd('cat /log')
    return data
    # TODO Parse XML

def startIperfClient(host, serverIP, duration):
    cmd = 'iperf -c {0} -i 1 -t {1} > /iperf &'.format(serverIP, duration)
    print cmd
    host.sendCmd(cmd)

def startIperfServer(server):
    server.sendCmd('iperf -s &')

def killPlayer(host):
    print host.cmd('pkill qtsampleplayer')


def getFile(node, path="/ndn/broadcast/ndnfs/videos/DashJsMediaFile/NDN.mpd"):
    print(node.cmd("ndngetfile {0}".format(path)))


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
    initExperiment(toptype=args['topology'][0], duration=args['duration'][0], algo=args['algo'][0], qoe_weights=args['qoe_weights'], 
                   use_memory=args['use_memory'], epoch=args['epoch'][0])
