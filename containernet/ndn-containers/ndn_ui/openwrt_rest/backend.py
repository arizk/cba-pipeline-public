import spur
import traceback
import sys
import csv
import json
import pandas as pd
import StringIO
import numpy as np
import re

BROADCAST = 'broadcast'
BEST_ROUTE = 'best-route'
#CLIENT_MAC = '"60:03:08:A5:89:60"'
CLIENT_MAC = '"B4:CE:F6:07:53:FF"'

def parseWifi(res):
  if res != "":
    result = []
    out =  res.splitlines()
    for el in out:
      print el
      #http://stackoverflow.com/questions/4289331/python-extract-numbers-from-a-string
      res_nmbers = re.findall(r'\d+', el[17:])
      result.append({
              'mac' : el[0:17],
              'snr' : res_nmbers[2],
              'last_seen' : res_nmbers[3]
              })
    return result
  else:
      return ""

def getWifiUers(routers):
    try:
        # incoming/outgoing Interest/Data counts
        router1 = spur.SshShell(hostname="192.168.1.1", username="root", password="ndnndn")
        with router1:
            print "getting users" + "iwinfo wlan1 assoclist | grep {0}".format(CLIENT_MAC)
            res = router1.run(["sh", "-c", "iwinfo wlan0 assoclist | grep {0}".format(CLIENT_MAC)], allow_error=True)
            routers[0]['props']['wifiusers'] = parseWifi(res.output)

        router2 = spur.SshShell(hostname="192.168.1.2", username="root", password="ndnndn")
        with router2:
            print "getting users" + "iwinfo wlan1 assoclist | grep {0}".format(CLIENT_MAC)
            res = router2.run(["sh", "-c", "iwinfo wlan1 assoclist | grep {0}".format(CLIENT_MAC)], allow_error=True)
            routers[1]['props']['wifiusers'] = parseWifi(res.output)
        return routers
    except:
        print(traceback.format_exc())
        print(sys.exc_info()[0])

def run_ndn():
    try:
        router1 = spur.SshShell(hostname="192.168.1.1", username="root", password="ndnndn")
        with router1:
            router1.run(["sh", "-c", "cd /etc/ndn && nohup nfd --config nfd.conf.sample > /dev/null 2>&1 &"], allow_error=True)
            res = router1.run(["sh", "-c", "nfd-status"], allow_error=True)
            res1 =  res.output + res.stderr_output
        router2 = spur.SshShell(hostname="192.168.1.2", username="root", password="ndnndn")
        with router2:
            res = router2.run(["sh", "-c", "cd /etc/ndn && nohup nfd --config nfd.conf.sample > /dev/null 2>&1 &"], allow_error=True)
            res = router2.run(["sh", "-c", "nfd-status"], allow_error=True)
            return res1 + res.output + res.stderr_output
    except:
        pass

def run_nfdc_1():
    try:
        router1 = spur.SshShell(hostname="192.168.1.1", username="root", password="ndnndn")
        with router1:
            res = router1.run(["sh", "-c", "~/launch_nfd_router1.sh"], allow_error=True)
            return res.output + res.stderr_output
    except:
        pass

def run_nfdc_2():
    try:
        router2 = spur.SshShell(hostname="192.168.1.2", username="root", password="ndnndn")
        with router2:
            res = router2.run(["sh", "-c", "~/launch_nfd_router2.sh"], allow_error=True)
            return res.output + res.stderr_output
    except:
        pass

def set_broadcast(routers):
    try:
        if routers[0]['props']['current_strategy'] == BEST_ROUTE:
            router1 = spur.SshShell(hostname="192.168.1.1", username="root", password="ndnndn")
            with router1:
                print "setting to broadcast"
                res = router1.run(["sh", "-c", "nfdc set-strategy ndn:/ndn/broadcast/ndnfs ndn:/localhost/nfd/strategy/broadcast"], allow_error=True)
                routers[0]['props']['message']= res.output + res.stderr_output

            router2 = spur.SshShell(hostname="192.168.1.2", username="root", password="ndnndn")
            with router2:
                print "setting to broadcast"
                res = router2.run(["sh", "-c", "nfdc set-strategy ndn:/ndn/broadcast/ndnfs ndn:/localhost/nfd/strategy/broadcast"], allow_error=True)
                routers[1]['props']['message'] = res.output + res.stderr_output
            routers[0]['props']['current_strategy']  = BROADCAST
            routers[1]['props']['current_strategy']  = BROADCAST
        else:
            print 'broadcast strategy already set'
    except:
        print(traceback.format_exc())
        print(sys.exc_info()[0])
        pass

##TODO: add costs to route
def set_best_route(routers):
    try:
        if routers[0]['props']['current_strategy'] == BROADCAST:
            router1 = spur.SshShell(hostname="192.168.1.1", username="root", password="ndnndn")
            with router1:
                print "setting to best route"
                res = router1.run(["sh", "-c", "nfdc set-strategy ndn:/ndn/broadcast/ndnfs ndn:/localhost/nfd/strategy/best-route"], allow_error=True)
                routers[0]['props']['message'] = res.output + res.stderr_output

            router2 = spur.SshShell(hostname="192.168.1.2", username="root", password="ndnndn")
            with router2:
                print "setting to best route"
                res = router2.run(["sh", "-c", "nfdc set-strategy ndn:/ndn/broadcast/ndnfs ndn:/localhost/nfd/strategy/best-route"], allow_error=True)
                routers[1]['props']['message'] = res.output + res.stderr_output
            routers[0]['props']['current_strategy'] = BEST_ROUTE
            routers[1]['props']['current_strategy'] = BEST_ROUTE
        else:
            print 'best route already set'
    except:
        pass
        print(traceback.format_exc())
        print(sys.exc_info()[0])

#http://stackoverflow.com/questions/3368969/find-string-between-two-substrings
def find_between( s, first, last ):
    try:
        start = s.index( first ) + len( first )
        end = s.index( last, start )
        return s[start:end]
    except ValueError:
        return ""

def find_between_r( s, first, last ):
    try:
        start = s.rindex( first ) + len( first )
        end = s.rindex( last, start )
        return s[start:end]
    except ValueError:
        return ""

def parseNDNResult(res):
    result = []
    out =  map(lambda x: x.split(" "), res.output.splitlines())
    for el in out:
        del el[0:3]
        del el[1]
        del el[-3:]
        #el[0] = el[0].lstrip("faceid=")
        ip = el[0].lstrip("remote=tcp4://").split(':', 1)[0]
        result.append({
            "id": ip.split('.')[-1],
            "ip": ip,
            "interst_in" : find_between(el[1], "counters={in={", "i"),
            "data_in" : el[2].rstrip("d"),
            "interst_in_bytes" : el[3].rstrip("B}"),
            "interst_out" : find_between(el[4], "out={", "i"),
            "data_out" :  el[5].rstrip("d"),
            "interst_out_bytes" : el[6].rstrip("B}}")
        })
    return result
def get_users(routers):
    try:
        # incoming/outgoing Interest/Data counts
        router1 = spur.SshShell(hostname="192.168.1.1", username="root", password="ndnndn")
        with router1:
            print "getting users"
            res = router1.run(["sh", "-c", "~/face_status.sh"], allow_error=True)
            result_router1 = parseNDNResult(res)
            df = pd.DataFrame(result_router1)
            df.drop_duplicates(subset="ip", inplace=True)
            routers[0]['props']['users'] = df.to_dict(orient="records")

        router2 = spur.SshShell(hostname="192.168.1.2", username="root", password="ndnndn")
        with router2:
            print "getting users"
            res = router2.run(["sh", "-c", "~/face_status_2.sh"], allow_error=True)
            result_router2 = parseNDNResult(res)
            df = pd.DataFrame(result_router2)
            df.drop_duplicates(subset="ip", inplace=True)
            routers[1]['props']['users'] = df.to_dict(orient="records")

    except:
        pass
        print(traceback.format_exc())
        print(sys.exc_info()[0])

### macro commands

def start_ndn():
    #todo get output, return to api
    #print "Running NDN Start"
    #not needed as it is launch at start
    #print run_ndn()
    print "Running NFDC1"
    print run_nfdc_1()
    print "Running NFDC2"
    print run_nfdc_2()
    print "Setting broadcast"
