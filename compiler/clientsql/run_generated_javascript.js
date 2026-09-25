'use strict';

const path = require('node:path');

if (process.argv.length !== 3) {
  throw new Error('Expected one generated JavaScript entrypoint');
}

require(path.resolve(process.argv[2]));
