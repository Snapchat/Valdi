export type FileEncoding = 'utf8' | 'utf16';

export interface ReadFileOptions {
  encoding?: FileEncoding | undefined | null;
}

export type WriteFileData = ArrayBuffer | Uint8Array | string;

/**
 * Valdi File System module
 * This FS API right now is only for internal usage due to some limitations.
 * Inside Valdi, we use it mostly infrastructure things.
 * If you have any questions about the usage this module please ask in the support channel
 */
export interface FileSystemModule {
  removeSync(path: string): boolean;

  createDirectorySync(path: string, createIntermediates: boolean): boolean;

  readFileSync(path: string, options?: ReadFileOptions): string | ArrayBuffer;

  writeFileSync(path: string, data: WriteFileData): void;

  currentWorkingDirectory(): string;
}
