#!/usr/bin/env node
// Copies ai-skills/ into bundled-skills/ inside the CLI package so skills are
// available at runtime without any network requests.
'use strict';

const fs = require('fs');
const path = require('path');

const src = path.resolve(__dirname, '..', '..', '..', 'ai-skills');
const dest = path.resolve(__dirname, '..', 'bundled-skills');

if (!fs.existsSync(src)) {
  console.log(`bundle-skills: ${src} not found, skipping`);
  process.exit(0);
}

if (fs.existsSync(dest)) {
  fs.rmSync(dest, { recursive: true });
}

fs.cpSync(src, dest, { recursive: true });
console.log(`bundle-skills: copied ai-skills/ → bundled-skills/`);
