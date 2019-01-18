# Links

https://github.com/react-webpack-generators/generator-react-webpack
http://visjs.org/index.html
https://exchange.nagios.org/directory/Addons/APIs/Nagira--2D-Nagios-RESTful-API/details
https://sudoroom.org/wiki/Mesh/Monitoring#OpenWRT_Package:_nrpe
https://github.com/firehol/netdata


# Router

### Installing the launch script
```
scp ~/git/ndn-demo/ndn_ui/openwrt_rest/ndn ndn1:/etc/init.d/
>ndn1: /etc/init.d/ndn start
>ndn1: /etc/init.d/ndn enable

./wrtbwmon setup
```

### Installing bwmonitor
```
wget https://gist.githubusercontent.com/heyeshuang/9305089/raw/cf74e373c0a9f1fc0809bc1abfe2a759ae9eedb3/wrtbwmon --no-check-certificate
chmod +x wrtbwmon
./wrtbwmon setup
./wrtbwmon update usage.db
```
