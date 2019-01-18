# ToDo: Put the existing scripts here

Tools:

NDNFS: https://github.com/remap/ndnfs-port
NFD: http://named-data.net/doc/NFD/current/INSTALL.html


# Setup Router
```
scp start_ndn_fs.sh ndn kom@192.168.1.116:~/
scp -r tears_of_steel  kom@192.168.1.190:~/

#on raspberry pi

chmod +x ndn start_ndn_fs.sh
sudo cp ndn /etc/init.d/
sudo update-rc.d ndn defaults

sudo a2enmod headers
sudo cp 000-default.conf /etc/apache2/sites-enabled/
sudo mv tears_of_steel /var/www/html/
sudo reboot

```
