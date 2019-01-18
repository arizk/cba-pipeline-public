import React, {Component} from 'react';
import DASH from 'dashjs';
import _ from 'lodash';

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

const servers = [
  {name: 'localhost', url: 'tears_of_steel/ondemand/stream.mpd'},
  {name: '192.168.1.116', url: 'http://192.168.1.116/tears_of_steel/ondemand/stream.mpd'},
  {name: '192.168.1.190', url: 'http://192.168.1.190/tears_of_steel/ondemand/stream.mpd'},
  {name: '192.168.1.118', url: 'http://192.168.1.118/tears_of_steel/ondemand/stream.mpd'},
  {name: '192.168.1.142', url: 'http://192.168.1.142/tears_of_steel/ondemand/stream.mpd'}
];

let player = null;
export class DASHPlayer extends Component {

  componentDidMount() {
    const self = this;
    $("#select-server").on("click", "li", function () {
      console.log($(this).attr('data'));
      $("#current-server").val($(this).attr('data'));
      self.initplayer($("#current-server").val());
    });
    /*
    $('#play').onClick(el => {
      if (self.player !== null)
        player.play();
    });
    */
  }

  initplayer(url) {
    // const url = "http://192.168.1.190/tears_of_steel/ondemand/stream.mpd";
    player = DASH.MediaPlayer().create();
    console.log("url is " + url);
    player.initialize(document.querySelector("#videoPlayer"), url, false);
    player.pause();
    player.setVolume(0);
  }

  render() {
    const serverList = servers.map(server =>
      <li data={server.url} key={server.name}><a href="#">{server.name}</a></li>
    );

    return (
      <div style={styles.container}>
        <main style={styles.main}>
          <div className="row">
            <div className="col-lg-6">
              <div className="btn-toolbar bs-component">
              <div className="input-group">
                <span className="input-group-btn">
                  <button id="select-source" type="button" className="btn btn-group-sm dropdown-toggle" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">Select source</button>
                  <ul id="select-server" className="dropdown-menu">
                    {serverList}
                  </ul>
                </span>
                  <input type="text" id="current-server" className="form-control control"/>
                </div>
                </div>
          </div>
          </div>
          <div className="row">
            <div className="col-md-12">
              <div className="embed-responsive embed-responsive-16by9">
                <video className="video embed-responsive-item" id="videoPlayer" controls>No video available</video>
              </div>
            </div>
          </div>
        </main>
      </div>
    );
  }
}
