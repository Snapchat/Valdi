const path = require('path');
const fs = require('fs');

const config = JSON.parse(fs.readFileSync(path.join(process.cwd(), 'tests', 'test_config.json'), 'utf8'));
const runner = require('valdi-web-devtools/test_utils/test_runner');

runner.run(config);
