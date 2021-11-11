'use strict';

const express = require('express');
const app = express();

app.use(express.static('.'));

app.listen(3000);
console.log('Nova web client started on http://localhost:3000');
