import React, {Component} from 'react';
import $ from 'jquery';

const styles = {
  header: {
    display: 'flex',
    alignItems: 'center',
    backgroundColor: 'cadetblue'
  },
  title: {
    flex: 1,
    fontSize: '2rem',
    margin: '1rem'
  }
};

function
  toggleButtons() {
  $("changeStrategyBroadcast").toggleClass("active");
  $("changeStrategyBest").toggleClass("active");
}

export class Header extends Component {
  constructor() {
    super();
    this.state = {data: []};
  }

  handleStartNFD() {
    console.log("Button start nfd clicked");
    fetch("http://127.0.0.1:5000/routers/start");
  }

  handleChangeStrategyBroadcast() {
    // toggleButtons();
    console.log("Button broadcast clicked");
    fetch("http://127.0.0.1:5000/routers/strategy_broadcast");
  }

  handleChangeStrategyBest() {
    console.log("Button best clicked");
    // toggleButtons();
    fetch("http://127.0.0.1:5000/routers/strategy_best");
  }

  render() {
    return (
      <header style={styles.header}>
        <p style={styles.title}>
            NDN Demo
        </p>
        <div className="btn-group changeStrategyBroadcast">
          <button onClick={this.handleChangeStrategyBroadcast} className="btn button active btn-primary">
              Broadcasting strategy
          </button>
          <button onClick={this.handleChangeStrategyBest} className="btn button active btn-primary changeStrategyBest">
              Best route strategy
          </button>
        </div>
        <button onClick={this.handleStartNFD} className="btn-space btn button">
          <b>
            Start NFD!
          </b>
        </button>
      </header>
    );
  }
}
