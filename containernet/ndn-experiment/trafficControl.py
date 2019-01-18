import time
import re
import pandas as pd
import re
from dynamicLinkChange import *
import locks

def traffic_control(from_host, to_host, switch, urlOrLocal, trace_file, trace_start_time = 0, duration_s = None, client='client1', epoch=-1):
    print('=======================================')
    print "Starting trace playback!"
    print epoch
    # TODO Remove client since we modulate middle links now
    with open('bandwidth_traces/{}_epoch{}.csv'.format(client, epoch), 'r') as f:
        lines = [line.split(',') for line in f]
        for bw, bw_period in lines:
            if locks.STOP_TC:
                break
            # Double bandwidth because we have 2 clients
            # TODO this makes a lot of assumptions
            print time.time()
            reconfigureConnection(to_host, switch, float(bw))
            time.sleep(float(bw_period))
    print "Done playing bandwidth trace."
    print('=======================================')
    return()

def getTCInfo(host, switch):
    links = host.connectionsTo(switch)

    link_tuple = links[0]
    hostIntf = link_tuple[0]
    switchIntf = link_tuple[1]
    string = switchIntf.tc('%s -s -d class show dev %s')
    print string
    matcher = re.compile(r'dropped ([\d]+)')
    return matcher.search(string).group(1)


def play_bandwidth_trace(client, server, switch, playback_thread, playback_starting_event, trace_url, trace_start_time = 0, result_queue=None):
  trace_data = read_trace_file_url(trace_url)

  print "* Starting Traffic Control from " + client.name + " to  " + server.name

  # When playback actually starts, we start recording
  playback_starting_event.wait()

  print '* Playing bandwidth trace'

  network_trace_recordings = []
  start_time = time.time()

  # For now, we switch for every measurement
  switch_interval = 1
  for index, row in trace_data.iloc[int(trace_start_time)::switch_interval, :].iterrows():
    if not playback_thread.is_alive():
      break

    bandwidth = float(row['bitrate'])
    network_trace_recordings.append((time.time() - start_time, bandwidth))
    reconfigureConnection(client, switch, bandwidth)
    time.sleep(float(row['elapsed_ms']/1000))

  playback_exceeded_trace_flag = False

  # If the playback thread is still alive, this means the playback is still running
  # and we are done with our trace playback
  # Set the flag signaling this and wait for the playback to be done
  if playback_thread.is_alive():
      playback_exceeded_trace_flag = True
      while playback_thread.is_alive():
          time.sleep(1)


  print '* Stopped playing bandwidth trace'

  # Send measures to result_queue (if present)
  if result_queue:
      result_dict = {
          'trace_adaptation_data': network_trace_recordings,
          'exceeded_trace_flag': playback_exceeded_trace_flag,
          'dropped_packets': getDroppedPackets(client, switch)
        }
      result_queue.put(result_dict)

  return network_trace_recordings

def getDroppedPackets(host, switch):
  links = host.connectionsTo(switch)

  link_tuple = links[0]
  switchIntf = link_tuple[1]

  tc_cmd ='%s -s -d class show dev %s'
  output_string = switchIntf.tc(tc_cmd)

  matcher = re.compile(r'dropped ([\d]+)')
  dropped_packets_match = matcher.search(output_string)
  return dropped_packets_match.group(1)

def read_trace_file_url(trace_file_url):
    colnames = ['Timestamp','Increasing time','GPS_1','GPS_2', 'bytes', 'elapsed_ms']
    data = pd.read_csv(trace_file_url, names=colnames, delim_whitespace=True)
    data['bitrate'] = data.bytes * 8 /  (data.elapsed_ms * 1000 )# mbit/s
    data.Timestamp = pd.to_datetime(data.Timestamp)
    return(data)

def read_trace_file_local(trace_file_local):
    colnames = ['start','end', 'elapsed_s', 'bitspersec']
    data = pd.read_csv(trace_file_local, names=colnames, delim_whitespace=True)
    data['bitrate'] = data.bitspersec / (1000 * 1000)# mbit/s
    data['elapsed_ms'] = data.elapsed_s * 1000
    return(data)
