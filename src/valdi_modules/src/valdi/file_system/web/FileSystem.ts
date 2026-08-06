import { FileSystemModule, ReadFileOptions, WriteFileData } from '../src/FileSystemModule';

const encUtf8 = new TextEncoder();
const decUtf8 = new TextDecoder();

/** UTF-16LE encode/decode (simple, BOM-less) */
function encodeUtf16LE(str: string): Uint8Array {
  const out = new Uint8Array(str.length * 2);
  for (let i = 0; i < str.length; i++) {
    const code = str.charCodeAt(i);
    out[i * 2] = code & 0xff;
    out[i * 2 + 1] = code >> 8;
  }
  return out;
}
function decodeUtf16LE(buf: Uint8Array): string {
  let s = "";
  const len = buf.length & ~1; // even length
  for (let i = 0; i < len; i += 2) {
    s += String.fromCharCode(buf[i] | (buf[i + 1] << 8));
  }
  return s;
}

const ROOT = "/";
function norm(p: string): string {
  // POSIX-like normalization; treat relative as from ROOT
  const parts = (p.startsWith("/") ? p : ROOT + p).split("/").filter(Boolean);
  const stack: string[] = [];
  for (const part of parts) {
    if (part === ".") continue;
    if (part === "..") { stack.pop(); continue; }
    stack.push(part);
  }
  return ROOT + stack.join("/");
}
function parentDir(p: string): string {
  if (p === ROOT) return ROOT;
  const idx = p.lastIndexOf("/");
  return idx > 0 ? p.slice(0, idx) : ROOT;
}

/* -------- in-memory FS state -------- */

const dirs = new Set<string>([ROOT]);
const files = new Map<string, Uint8Array>();
let CWD = ROOT;

function toBytes(data: WriteFileData): Uint8Array {
  if (typeof data === "string") return encUtf8.encode(data);
  if (data instanceof Uint8Array) return data.slice();
  return new Uint8Array(data).slice();
}

/* -------- implementation -------- */

const fsStub: FileSystemModule = {
  existsSync(path: string): boolean {
    const p = norm(path);
    return files.has(p) || dirs.has(p);
  },

  removeSync(path: string): boolean {
    const p = norm(path);
    if (files.delete(p)) return true;
    if (dirs.has(p)) {
      // recursive remove directory
      let removed = false;
      // remove all nested files/dirs
      for (const f of Array.from(files.keys())) {
        if (f === p || f.startsWith(p + "/")) {
          files.delete(f);
          removed = true;
        }
      }
      for (const d of Array.from(dirs.values())) {
        if (d !== ROOT && (d === p || d.startsWith(p + "/"))) {
          dirs.delete(d);
          removed = true;
        }
      }
      // finally remove the dir itself
      if (p !== ROOT) dirs.delete(p);
      return removed || true;
    }
    return false;
  },

  createDirectorySync(path: string, createIntermediates: boolean): boolean {
    const p = norm(path);
    if (dirs.has(p)) return true;
    const chain: string[] = [];
    let cur = p;
    while (cur !== ROOT && !dirs.has(cur)) {
      chain.push(cur);
      cur = parentDir(cur);
    }
    if (!dirs.has(cur) && !createIntermediates) return false;
    if (!createIntermediates && parentDir(p) !== cur) return false;

    // create parents if requested/needed
    while (chain.length) {
      const d = chain.pop()!;
      dirs.add(d);
    }
    return true;
  },

  readFileSync(path: string, options?: ReadFileOptions): string | ArrayBuffer {
    const p = norm(path);
    const buf = files.get(p) ?? new Uint8Array(0);

    const enc = options?.encoding;
    if (enc === "utf8") return decUtf8.decode(buf);
    if (enc === "utf16") return decodeUtf16LE(buf);
    // undefined/null => raw bytes
    return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
  },

  writeFileSync(path: string, data: WriteFileData): void {
    const p = norm(path);
    // ensure parent dir exists (be friendly in the stub)
    const parent = parentDir(p);
    if (!dirs.has(parent)) {
      this.createDirectorySync(parent, true);
    }
    files.set(p, toBytes(data));
  },

  currentWorkingDirectory(): string {
    return CWD;
  },
};

/** Exported instance + setter so you can swap in a real native binding later. */
export let fileSystem: FileSystemModule = fsStub;
export function setFileSystem(impl: FileSystemModule): void {
  fileSystem = impl;
}
