import React, {Component} from 'react';
import {Header} from './header';
//ToDo correctly unilitailize players
import {NDNPlayer} from './ndn-player';
import {DASHPlayer} from './dashjs-ndn';

import '../lib/bootstrap.min.css';
import '../material/css-compiled/ripples.min.css';
import '../material/css-compiled/material-wfont.min.css';

import '../lib/bootstrap.min';
// require(["imports?$=jquery?script-loader!../material/scripts/ripples", 'imports?$=jquery?script-loader!../material/scripts/material']);

import '../material/scripts/ripples';
import '../material/scripts/material';

const styles = {
  container: {
    display: 'flex',
    flexDirection: 'column',
    minHeight: '100%'
  },
  main: {
    flex: 1,
    display: 'flex',
    flexDirection: 'column'
  }
};

export class Main extends Component {

  constructor() {
    super();
    this.state = {showNDN: true};
  }

  componentDidMount() {
    const self = this;
    $("#player_mode :input").change(function () {
      console.log($(this).attr('id'));
      if ($(this).attr('id') === "ndn-dash")
        self.setState({showNDN: true});
      else
      self.setState({showNDN: false});
    });
  }
  render() {
    return (
      <div style={styles.container}>
        <main style={styles.main}>
          <Header/>
           {this.state.showNDN && <NDNPlayer/>}
           {!this.state.showNDN && <DASHPlayer/>}
          </main>
        </div>
    );
  }
    }
