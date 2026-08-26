import { fs } from 'file_system/src/FileSystem';

function isAbsolutePath(path: string): boolean {
  return path.startsWith('/');
}

function normalizedParts(parts: string[], absolute: boolean): string[] {
  const out: string[] = [];
  for (const part of parts) {
    if (!part || part === '.') {
      continue;
    }
    if (part === '..') {
      if (out.length > 0 && out[out.length - 1] !== '..') {
        out.pop();
      } else if (!absolute) {
        out.push(part);
      }
    } else {
      out.push(part);
    }
  }
  return out;
}

export class Path {
  private readonly isAbsolutePath: boolean;
  private readonly isNormalized: boolean;
  private readonly parts: string[];

  private constructor(parts: string[], isAbsolute: boolean, normalized: boolean) {
    this.parts = parts;
    this.isAbsolutePath = isAbsolute;
    this.isNormalized = normalized;
  }

  static fromString(value: string): Path {
    const isAbsolute = isAbsolutePath(value);
    const parts = value.split('/');
    return new Path(parts, isAbsolute, false);
  }

  static currentWorkingDirectory(): Path {
    return Path.fromString(fs.currentWorkingDirectory());
  }

  normalize(): Path {
    if (this.isNormalized) {
      return this;
    }
    return new Path(normalizedParts(this.parts, this.isAbsolutePath), this.isAbsolutePath, true);
  }

  static isAbsolute(path: string): boolean {
    return isAbsolutePath(path);
  }

  static join(...parts: string[]): Path {
    return new Path(parts, false, false);
  }

  static resolve(...parts: string[]): Path {
    let out = Path.currentWorkingDirectory();
    for (const part of parts) {
      out = Path.isAbsolute(part) ? Path.fromString(part) : out.appending(part);
    }
    return out.normalize();
  }

  parent(): Path {
    if (this.isAbsolutePath && this.parts.length === 0) {
      return this;
    }
    if (!this.isAbsolutePath && this.parts.length <= 1) {
      return new Path(['.'], false, false);
    }
    const parts = this.parts.slice(0, this.parts.length - 1);
    return new Path(parts, this.isAbsolutePath, this.isNormalized);
  }

  basename(): string {
    if (this.parts.length === 0) {
      return this.isAbsolutePath ? '/' : '.';
    }
    return this.parts[this.parts.length - 1]!;
  }

  appending(...paths: string[]): Path {
    let out: Path = this;
    for (const path of paths) {
      if (Path.isAbsolute(path)) {
        out = Path.fromString(path);
      } else {
        const joinedParts = [...out.parts, ...path.split('/')];
        out = new Path(joinedParts, out.isAbsolutePath, false);
      }
    }
    return out;
  }

  ensureDirectory(): void {
    const value = this.toString();
    if (value === '/' || value === '.') {
      return;
    }
    fs.createDirectorySync(value, true);
  }

  exists(): boolean {
    return fs.existsSync(this.toString());
  }

  toString(): string {
    if (this.parts.length === 0) {
      return this.isAbsolutePath ? '/' : '.';
    }
    return `${this.isAbsolutePath ? '/' : ''}${this.parts.join('/')}`;
  }
}
