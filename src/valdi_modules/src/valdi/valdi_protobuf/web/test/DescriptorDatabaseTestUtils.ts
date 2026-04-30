import { DescriptorDatabase } from '../headless/DescriptorDatabase';

// Node fs/path imports - using require to avoid needing @types/node in tsconfig
declare function require(name: string): any;
declare const __dirname: string;
const fs = require('fs');
const path = require('path');

export function getLoadedDescriptorDatabase(): DescriptorDatabase {
  // Load proto.protodecl from the test directory
    // Path is relative to compiled location in web/test/, goes up 2 levels to valdi_protobuf/, then to test/
    const protoDeclPath = path.join(__dirname, '../../test/proto.protodecl');
  const protoDeclContent = fs.readFileSync(protoDeclPath);
  const database = new DescriptorDatabase();
  database.addFileDescriptorSet(new Uint8Array(protoDeclContent));
  database.resolve();
  return database;
}
