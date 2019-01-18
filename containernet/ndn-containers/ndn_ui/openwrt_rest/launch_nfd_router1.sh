#/bin/bash

/etc/init.d/ndn restart
sleep 5
cd /etc/ndn
nfdc register ndn:/ndn/broadcast/ndnfs tcp://192.168.1.2
nfdc register -c 1 ndn:/ndn/broadcast/ndnfs tcp://192.168.1.118
nfdc register -c 2 ndn:/ndn/broadcast/ndnfs tcp://192.168.1.116
nfdc register -c 2 ndn:/ndn/broadcast/ndnfs tcp://192.168.1.190
nfdc register -c 1 ndn:/ndn/broadcast/ndnfs tcp://192.168.1.142
