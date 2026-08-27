// Loads .bin/.protodecl resources as raw bytes (Uint8Array). The web runtime's
// resourceModuleToByteArray only accepts Uint8Array/ArrayBuffer, and webpack has no
// built-in byte-array module type, so emit a module that decodes base64 to bytes.
module.exports = function bytesLoader(content) {
  const base64 = content.toString('base64');
  return (
    `const s = atob(${JSON.stringify(base64)});` +
    'const bytes = new Uint8Array(s.length);' +
    'for (let i = 0; i < s.length; i++) { bytes[i] = s.charCodeAt(i); }' +
    'module.exports = bytes;'
  );
};
module.exports.raw = true;
