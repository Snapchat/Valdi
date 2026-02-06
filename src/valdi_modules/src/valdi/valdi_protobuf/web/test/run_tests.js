#!/usr/bin/env node
/**
 * Test runner for web protobuf tests using Jest
 */

const { spawnSync } = require('child_process');
const path = require('path');
const fs = require('fs');

// Bazel runfiles helper
const runfiles = process.env.RUNFILES_DIR ||  process.env.RUNFILES;
if (!runfiles) {
  console.error('RUNFILES_DIR not set');
  process.exit(1);
}

// Add node_modules to NODE_PATH so Jest can resolve dependencies
// Try multiple possible runfiles paths (local workspace vs external workspace referencing valdi)
const possiblePaths = [
  path.join(runfiles, 'valdi/src/valdi_modules/src/valdi/valdi_protobuf/node_modules'), // Local valdi workspace
  path.join(runfiles, '_main~local_repos~valdi/src/valdi_modules/src/valdi/valdi_protobuf/node_modules'), // External workspace referencing valdi
];

let nodeModulesPath = null;
for (const p of possiblePaths) {
  if (fs.existsSync(p)) {
    nodeModulesPath = p;
    break;
  }
}

if (!nodeModulesPath) {
  console.error(`Could not find node_modules in any of: ${possiblePaths.join(', ')}`);
  process.exit(1);
}

process.env.NODE_PATH = nodeModulesPath + (process.env.NODE_PATH ? ':' + process.env.NODE_PATH : '');
require('module').Module._initPaths();

// Find jest binary via runfiles
const jestBin = path.join(nodeModulesPath, 'jest/bin/jest.js');

if (!fs.existsSync(jestBin)) {
  console.error(`Jest binary not found at: ${jestBin}`);
  process.exit(1);
}

// The compiled test files are in the dist output directory
// Only test files from valdi_protobuf/web/test (out_dir is "web" so compiled files are at web/test)
const testDir = path.join(runfiles, 'valdi/src/valdi_modules/src/valdi/valdi_protobuf/web/test');

if (!fs.existsSync(testDir)) {
  console.error(`Test directory not found: ${testDir}`);
  process.exit(1);
}

// List test files explicitly - only .spec.js files in test/
const testFiles = fs.readdirSync(testDir)
  .filter(f => f.endsWith('.spec.js'))
  .map(f => path.join(testDir, f));

if (testFiles.length === 0) {
  console.error(`No .spec.js files found in ${testDir}`);
  process.exit(1);
}

console.log('Running tests:', testFiles);

// Run jest on the compiled test files directly
// Create a temp jest config to prevent auto-discovery
const jestConfig = {
  testEnvironment: 'node',
  testPathIgnorePatterns: [],  // Don't ignore anything
  roots: [testDir],  // Only search in test directory
  testMatch: ['**/*.spec.js'],  // Only match .spec.js files
};
const configPath = path.join(testDir, 'jest.config.json');
fs.writeFileSync(configPath, JSON.stringify(jestConfig));

console.log('Running tests:', testFiles);
const result = spawnSync('node', [
  jestBin,
  '--config', configPath,
  '--verbose',
  '--runTestsByPath',  // Only run the exact paths provided
  ...testFiles  // Pass test files directly
], {
  stdio: 'inherit',
  cwd: testDir  // Run from test directory
});

// Clean up config
try {
  fs.unlinkSync(configPath);
} catch (e) {
  // Ignore cleanup errors
}

process.exit(result.status || 0);
