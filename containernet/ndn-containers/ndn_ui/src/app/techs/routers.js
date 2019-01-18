import React, {Component} from 'react';
import 'underscore';
const cytoscape = require('cytoscape');
const _ = require('lodash');
const refreshInterval = 5000;
let cy = null;
const routerSize = 60;
const nodeSize = 30;
const vScale = 0.7;
const nodeStyle = {
  2: {position: {x: 400 * vScale, y: 300}, label: 'Router ', type: 1, color: '#fc8d59', size: routerSize},
  1: {position: {x: 1200 * vScale, y: 300}, label: 'Router ', type: 1, color: '#fc8d59', size: routerSize},
  116: {position: {x: 200 * vScale, y: 500}, label: 'Data ', type: 2, color: '#11479e', size: nodeSize},
  190: {position: {x: 600 * vScale, y: 500}, label: 'Data ', type: 2, color: '#11479e', size: nodeSize},
  118: {position: {x: 1000 * vScale, y: 500}, label: 'Data ', type: 2, color: '#11479e', size: nodeSize},
  142: {position: {x: 1400 * vScale, y: 500}, label: 'Data ', type: 2, color: '#11479e', size: nodeSize},
  wsclient: {position: {x: 800 * vScale, y: 100}, label: 'Client ', type: 3, color: '#f14791', size: nodeSize}
};

const links = [
  // links for raspberry pis are fixed
  // Todo: append links of conencted device
  {data: {id: '2-116', label: 'Cost: 1', weight: 1, source: '2', target: '116'}},
  {data: {id: '2-190', label: 'Cost: 1', weight: 1, source: '2', target: '190'}},
  {data: {id: '1-118', label: 'Cost: 1', weight: 1, source: '1', target: '118'}},
  {data: {id: '1-142', label: 'Cost: 1', weight: 1, source: '1', target: '142'}},
  {data: {id: '1-2', label: 'Cost: 1', weight: 1, source: '1', target: '2'}}
];

const styles = {
  container: {
    margin: '1rem'
  },
  h2: {
    fontWeight: 300,
    fontSize: '1.5rem'
  },
  techs: {
    display: 'flex',
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-around'
  }
};

export class Routers extends Component {
  constructor() {
    super();
    this.state = {};
  }

  initPlot() {
    cy = cytoscape({
      container: document.getElementById('cy'),

      userZoomingEnabled: true,
      panningEnabled: true,
      userPanningEnabled: true,
      motionBlur: true,
      fit: true,
      boxSelectionEnabled: false,
      autounselectify: true,
      layout: {
        name: 'preset'
      },

      style: cytoscape.stylesheet()
    .selector('node')
    .css({
      "label": 'data(name)',
      'text-opacity': 0.5,
      'background-color': 'data(color)',
      'width': 'data(size)',
      'height': 'data(size)',
      'text-wrap': 'wrap'
    })
    .selector('edge')
    .css({
      'width': 4,
      'target-arrow-shape': 'triangle',
      'line-color': '#9dbaea',
      'target-arrow-color': '#9dbaea',
      'curve-style': 'bezier',
      "label": 'data(label)'
    }),
      elements: {
        nodes: [],
        edges: []
      }
    });
  }

  addLink(nodeId) {
    _.forEach(links, link => {
      if (link.data.target === String(nodeId)) {
        console.log(link, nodeId);
        if (cy.$(`[id="${link.data.id}"]`).isEdge()) {
          cy.$(`[id="${link.data.id}"]`).data('data', link.data);
        } else {
          cy.add({
            group: "edges",
            data: link.data
          });
        }
        console.log(`edge with id="${link.data.id} exists already`);
      }
    });
  }

  fetchWiFi() {
    // check that cy is inited and a wsclient is connected
    if (cy === null || !cy.$(`[id="wsclient"]`).inside())
      return;

    fetch("http://localhost:5000/routers/wifiusers")
    .then(response => response.json())
    .then(json => {
      const nodesRes = _.reject(json, el => {
        return el.props.wifiusers === "";
      });
      const nodes = _.minBy(nodesRes, el => {
        return _.minBy(el.props.wifiusers, wifi => {
          console.log(wifi);
          return wifi.last_seen;
        });
        // todo filter only most recent node
      });
      // remove old wifi edges cy.getElementById( id )
      console.log(nodes);
      if (!cy.$(`[id="${nodes.id}-wsclient"]`).isEdge()) {
        // remove both edges from routers
        cy.$(`[id="1-wsclient"]`).remove();
        cy.$(`[id="2-wsclient"]`).remove();
        cy.add({
          group: "edges",
          data: {
            weight: 75,
            source: 'wsclient',
            target: nodes.id,
            id: `${nodes.id}-wsclient`,
            name: nodes.mac
          }
        });
      }
    });
  }

  fetchUsers() {
    if (cy === null)
      return;
    fetch("http://127.0.0.1:5000/routers/users")
  .then(response => response.json())
  .then(json => {
    // only nodes with users
    let nodesRes = _.reject(json, el => {
      return el.props.users === "";
    });
    // ????
    nodesRes = _.each(_.flatMap(nodesRes, el => {
      return el.props.users;
    }), nodes => {
      console.log(nodes);
      if (!cy.$(`[id="${nodes.id}"]`).isNode()) {
        cy.add({
          group: "nodes",
          data: {id: nodes.id,
name: `${_.get(nodeStyle, [nodes.id]).label}
IP: ${nodes.ip}
Interest Requests (in): ${nodes.interst_in}
Interest Requests (out): ${nodes.interst_out}
Data Requests: ${nodes.data_in}`,
// Data Requests (out): ${nodes.data_out},
                    size: _.get(nodeStyle, [nodes.id]).size,
                    color: _.get(nodeStyle, [nodes.id]).color
                  },
          position: _.get(nodeStyle, [nodes.id]).position});
      }
      this.addLink(nodes.id);
    });
  });
  }

  componentWillMount() {

  }

  componentDidMount() {
    setInterval(() => {
      this.setState({
        interval: this.state.interval + 1
      });
    }, refreshInterval);
    this.initPlot();
  }

  render() {
    /**
    const routers = this.state.data.map(router => {
      console.log(router);
      return <p key={router.id}>{router.props.users.toString()}</p>;
    });
    **/

    return (
      <div style={styles.container}>
        <div id="cy"></div>
      </div>
      );
  }

  componentDidUpdate() {
    // console.log(this.state.nodes);
    this.fetchUsers();
    this.fetchWiFi();
  }
  }
