import React, {Component} from 'react';
import {Header} from './header';
import {Routers} from './techs/Routers';
import {Footer} from './footer';

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

  render() {
    return (
      <div style={styles.container}>
        <Header/>
        <main style={styles.main}>
          <Routers/>
        </main>
        <Footer/>
      </div>
    );
  }
}
