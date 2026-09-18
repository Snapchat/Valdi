/**
 * ALL NATIVE MODULES MUST BE REGISTERED
 * 
 * Native module files get output to valdi_core/src/ModuleName
 * But can be referenced in multiple ways:
 *      require('ModuleName') => registerModule('ModuleName')
 *      import { Thing } from './ModuleName' => registerModule('path/to/parent/ModuleName')
 */

var valdiTsx = require('valdi_gpt_npm/native/valdi_tsx/web/JSX')
global.moduleLoader.registerModule('valdi_tsx/src/JSX', () => {
  return valdiTsx;
});

