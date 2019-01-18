#!/usr/bin/python

"""

"""

from mininet.net import Containernet
from mininet.node import Controller, Docker, OVSSwitch
from mininet.cli import CLI
from mininet.log import setLogLevel, info
from mininet.link import TCLink, Link
from mininet.util import dumpNodeConnections
import fnss
import time
import re
from IPython import embed


class NDNContainernet( Containernet ):
    """
    A Mininet with Docker related methods.
    Inherits Mininet.
    This class is not more than API beautification.
    """

    def __init__(self, **params):
        # call original Mininet.__init__
        Containernet.__init__(self, **params)

    def buildFromTopo( self, topo=None ):
        """Build mininet from a topology object
           At the end of this function, everything should be connected
           and up."""

        # Possibly we should clean up here and/or validate
        # the topo
        if self.cleanup:
            pass

        info( '*** Creating network\n' )

        if not self.controllers and self.controller:
            # Add a default controller
            info( '*** Adding controller\n' )
            classes = self.controller
            if not isinstance( classes, list ):
                classes = [ classes ]
            for i, cls in enumerate( classes ):
                # Allow Controller objects because nobody understands partial()
                if isinstance( cls, Controller ):
                    self.addController( cls )
                else:
                    self.addController( 'c%d' % i, cls )

        info( '*** Adding docker hosts:\n' )
        for hostName in topo.hosts():
            self.addDocker( hostName, **topo.nodeInfo( hostName ))
            info( hostName + ' ' )

        info( '\n*** Adding switches:\n' )
        for switchName in topo.switches():
            # A bit ugly: add batch parameter if appropriate
            params = topo.nodeInfo( switchName)
            cls = params.get( 'cls', self.switch )
            #if hasattr( cls, 'batchStartup' ):
            #    params.setdefault( 'batch', True )
            self.addSwitch( switchName, **params )
            info( switchName + ' ' )

        info( '\n*** Adding links:\n' )
        for srcName, dstName, params in topo.links(
                sort=True, withInfo=True ):
            self.addLink( **params )
            info( '(%s, %s) ' % ( srcName, dstName ) )

        info( '\n' )


def topology():


    fnss_topology = fnss.parse_topology_zoo('AttMpls.gml')
    #fnss.two_tier_topology(1, 2, 2)
    "Create a network with some docker containers acting as hosts."

    # Set link attributes
    # https://fnss.github.io/doc/core/apidoc/fnss.functions.html
    #fnss.set_capacities_constant(fnss_topology, 10, 'Mbps')
    #fnss.set_delays_constant(fnss_topology, 2, 'ms')
    #fnss.set_buffer_sizes_constant(fnss_topology, 50, 'packets')
    fnss.set_delays_geo_distance(fnss_topology, specific_delay=fnss.PROPAGATION_DELAY_FIBER)
    

    mn_topo = fnss.to_mininet(fnss_topology, relabel_nodes=True)
    for node in mn_topo.hosts():
        mn_topo.setNodeInfo(node, {
                                    "dcmd" : ["/bin/bash", "/ndn-entrypoint.sh"], 
                                    "dimage" : "ndnrepo_ndn:latest",
                                    "privileged" : True,
                                    "cls" : Docker
                                    } )
        #mn_topo.setNodeInfo(node, "privileged", True )
        #mn_topo.setNodeInfo(node, "dimage", "ndnrepo_ndn:latest" )
        #node.dcmd=["/bin/bash", "/ndn-entrypoint.sh"]
        # = Docker('{0}'.format(node), ip='10.0.0.{0}'.format(node), , privileged=True, dimage="ndnrepo_ndn:latest")
        #node.type='host'
        #print node
        #nodes.append(node)

    net = NDNContainernet(topo=mn_topo, link=TCLink, controller=Controller)



    dumpNodeConnections(net.hosts)
    dumpNodeConnections(net.switches)
    fnss_topology.edges()
    info('*** Starting network\n')
    net.start()
    embed()


    #TODO Add NDN Links for all
    #fnss_topology.edges()
    #[(0, 1), (0, 2), (1, 3), (1, 4), (2, 5), (2, 6)]
    #addNDNRoute(d1, d2)

    #net = Containernet(controller=Controller)

    info('*** Adding controller\n')
    #net.addController('c0')

    info('*** Adding docker containers\n')
    #d1 = net.addDocker('d1', ip='10.0.0.251', dcmd=["/bin/bash", "/ndn-entrypoint.sh"], privileged=True, dimage="ndnrepo_ndn:latest")
    #d2 = net.addDocker('d2', ip='10.0.0.250', dcmd=["/bin/bash", "/ndn-entrypoint.sh"], privileged=True, dimage="ndnrepo_ndn:latest")

    #s1 = net.addSwitch('s1')

    info('*** Creating links\n')
    #net.addLink(d1, s1)
    #net.addLink(s1, d2)

    

    
    time.sleep(5)
    print addNDNRoute(d1, d2)
    #TODO create file when inserting is done
    while not (checkRepoNGInitDone(d1) and checkRepoNGInitDone(d2)):
        time.sleep(5)
    
    print listFilesInRepo(d1)
    print listFilesInRepo(d2) 

    info('*** Running CLI\n')
    dumpNodeConnections(net.hosts)

    CLI(net)

    info('*** Stopping network')
    net.stop()

def addNDNRoute(source, dest, cost=1):
    return source.cmd("nfdc register -c {0} ndn:/ndn/broadcast/ndnfs tcp://{1}".format(cost, dest.IP()))
    
def listFilesInRepo(node):
    return node.cmd('repo-ng-ls')

def listLogs(node):
    NFD_LOG="/var/log/nfd.out"
    NFD_ERROR_LOG="/var/log/nfd-error.out"

    NDN_COPY_LOG="/var/log/copy-repo.log"
    NDN_COPY_ERROR="/var/log/copy-repo-error.log"

    NDNFS_LOG="/var/log/ndnfs.log"
    NDNFS_SERVER_LOG="/var/log/ndnfs-server.log"

    REPO_NG_LOG="/var/log/repong.log"
    REPONG_ERROR_LOG="/var/log/repong-error.log"

    return node.cmd('tail {0} {1} {2}'.format(REPO_NG_LOG, NFD_LOG, NDN_COPY_LOG))

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
    s = node.cmd('repo-ng-ls')
    p = re.compile(r'^Total number of data = +(\d+)', re.M)
    try:
        status = re.findall(p, s)[0]
    except:  
        status = 0
    if int(status) == 88460:
        return True
    else:
        print status
        return False

def listFiles(node):
    print node.cmd("find /videos/")

def getFile(node, path="/ndn/broadcast/ndnfs/videos/DashJsMediaFile/NDN.mpd"):
    print(node.cmd("ndngetfile {0}".format(path)))



if __name__ == '__main__':
    setLogLevel('info')
    topology()
