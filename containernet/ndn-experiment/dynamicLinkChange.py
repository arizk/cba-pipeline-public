def reconfigureConnection(host, switch, bandwidth = None, delay = None, loss = None):
  opts = {}

  if bandwidth is not None:
    opts['bw'] = bandwidth

  if delay is not None:
    opts['delay'] = str(delay) + 'ms'

  if loss is not None:
    opts['loss'] = loss

  if opts:
      setLinkOptions(host, switch, opts)

def setLinkOptions(host, switch, opts):
  links = host.connectionsTo(switch)

  link_tuple = links[0]
  switchIntf = link_tuple[1]

  bw = opts.get('bw')
  delay = opts.get('delay')
  loss = opts.get('loss')

  # Gets executed
  if bw:
    tc_change_cmd ='%s class change dev %s parent 5:0 classid 5:1 htb rate '+str(bw)+'Mbit burst 15k'
    switchIntf.tc(tc_change_cmd)
    
  # Does not get executed
  if opts.get('delay') or opts.get('loss'):
    parent = '5:1' if bw is not None else 'root'
    netem_change_cmd = '%s class change dev %s parent ' + parent + 'netem '

    if delay:
      netem_change_cmd += 'delay ' + opts.get('delay')

    if loss:
      netem_change_cmd += ' loss ' + opts.get('delay')

    switchIntf.tc(netem_change_cmd)
