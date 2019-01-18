import React, {Component} from 'react';

const styles = {
  footer: {
    padding: '0.5rem',
    fontSize: '1rem',
    backgroundColor: 'cadetblue',
    textAlign: 'center'
  }
};

export class Footer extends Component {
  render() {
    return (
      <footer style={styles.footer}>
        MAKI - TU-Darmstadt
      </footer>
    );
  }
}
