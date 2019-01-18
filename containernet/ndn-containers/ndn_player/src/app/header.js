import React, {Component} from 'react';

const styles = {
  header: {
    display: 'flex',
    alignItems: 'center',
    backgroundColor: '#1f1f1f'

  },
  title: {
    flex: 1,
    fontSize: '1.5rem',
    margin: '1rem',
    color: 'white'
  }
};

export class Header extends Component {
  constructor() {
    super();
    this.state = {data: []};
  }

  render() {
    return (
      <div className="row">
        <div className="col-md-12">
          <div id="player_mode" className="btn-group btn-group-justified" data-toggle="buttons">
            <label className="btn btn-primary active">
              <input type="radio" id="ndn-dash" name="options" autoComplete="off" defaultChecked/>Simple-NDN-DASH
            </label>
            <label className="btn btn-primary">
              <input type="radio" id="regular-dash" autoComplete="off" name="options"/>DASH.JS-NDN
            </label>
          </div>
        </div>
      </div>
    );
  }
}
