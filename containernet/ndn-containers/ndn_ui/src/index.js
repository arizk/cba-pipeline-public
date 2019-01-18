import 'babel-polyfill';

import React from 'react';
import ReactDOM from 'react-dom';

import {Main} from './app/main';
// import {Button} from 'react-bootstrap';
import 'bootstrap-material-design';
import './index.css';
import './app/css/bootstrap.min.css';
import './app/css/bootstrap-material-design.css';
import './app/css/ripples.css';

ReactDOM.render(
  <Main/>,
  document.getElementById('root')
);
