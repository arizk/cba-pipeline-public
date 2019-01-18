# ToDo: Put the existing scripts here

Steps to inable NDN routing:


Start NFD with:

$ sudo nfd --config nfd.conf


Register the file systems:

$ nfdc register ndn:/ndn/broadcast/ndnfs tcp://192.168.1.2


Set the routing strategy:

$ nfdc set-strategy ndn:/ndn/broadcast/ndnfs ndn:/localhost/nfd/strategy/broadcast



Full help at http://named-data.net/doc/NFD/current/manpages/nfdc.html




# OpenWRT Images

https://wiki.openwrt.org/toh/linksys/wrt1x00ac_series#tab__wrt1200ac4

SSID: OpenWRT Password: changeme
root password: changeme
