import 'jasmine/src/jasmine';
import { arrayToString } from 'coreutils/src/Uint8ArrayUtils';
import { fs, VALDI_MODULES_ROOT } from '../src/FileSystem';

describe('File System Module', () => {
  const testFolder = `${VALDI_MODULES_ROOT}/file_system/test`;
  const newFSItems = {
    file: `${testFolder}/new_test_file.txt`,
    folder: `${testFolder}/new_folder`,
  };

  function removeIfExists(path: string): void {
    if (fs.existsSync(path)) {
      fs.removeSync(path);
    }
  }

  beforeEach(() => {
    removeIfExists(newFSItems.file);
    removeIfExists(newFSItems.folder);
  });

  afterEach(() => {
    removeIfExists(newFSItems.file);
    removeIfExists(newFSItems.folder);
  });

  it('should read file with content inside', () => {
    const fileName = `${VALDI_MODULES_ROOT}/file_system/test/read_test_file.txt`;
    const fileContent = 'test data for file';

    fs.writeFileSync(fileName, fileContent);
    try {
      const result = fs.readFileSync(fileName, { encoding: 'utf8' });

      expect(result).toEqual(fileContent);
    } finally {
      fs.removeSync(fileName);
    }
  });

  it('should read file as array buffer', () => {
    const fileName = `${VALDI_MODULES_ROOT}/file_system/test/read_array_buffer_test_file.txt`;
    const fileContent = 'test data for file';

    fs.writeFileSync(fileName, fileContent);
    try {
      const result = fs.readFileSync(fileName);

      expect(result).toBeInstanceOf(ArrayBuffer);

      const stringContent = arrayToString(new Uint8Array(result as ArrayBuffer));

      expect(stringContent).toEqual(fileContent);
    } finally {
      fs.removeSync(fileName);
    }
  });

  it('should throw an error for read file operation if file does not exist', () => {
    const fileName = `${VALDI_MODULES_ROOT}/file_system/test/no_file`;

    expect(() => fs.readFileSync(fileName)).toThrowError(`Could not read the file at path: '${fileName}'`);
  });

  it('should create, write and remove the new file', () => {
    const fileContent = 'test data for file';
    fs.writeFileSync(newFSItems.file, fileContent);

    expect(fs.existsSync(newFSItems.file)).toBeTrue();

    const result = fs.readFileSync(newFSItems.file, { encoding: 'utf8' });

    expect(result).toEqual(fileContent);

    const resultFromFileRemove = fs.removeSync(newFSItems.file);

    expect(resultFromFileRemove).toBeTrue();
    expect(fs.existsSync(newFSItems.file)).toBeFalse();
  });

  it('should write Uint8Array content', () => {
    const fileName = `${VALDI_MODULES_ROOT}/file_system/test/write_uint8_array_test_file.txt`;
    const source = new Uint8Array([120, 116, 101, 115, 116, 32, 100, 97, 116, 97, 121]);
    const fileContent = source.subarray(1, source.length - 1);

    fs.writeFileSync(fileName, fileContent);
    try {
      const result = fs.readFileSync(fileName, { encoding: 'utf8' });

      expect(result).toEqual('test data');
    } finally {
      fs.removeSync(fileName);
    }
  });

  it('should get current root directory for client repository', () => {
    expect(() => fs.currentWorkingDirectory()).not.toThrow();
  });

  it('should create and remove a new folder', () => {
    fs.createDirectorySync(testFolder, true);

    const result = fs.createDirectorySync(newFSItems.folder, false);

    expect(result).toBeTrue();
    expect(fs.createDirectorySync(newFSItems.folder, true)).toBeTrue();
    expect(fs.existsSync(newFSItems.folder)).toBeTrue();

    const resultFromFolderRemove = fs.removeSync(newFSItems.folder);

    expect(resultFromFolderRemove).toBeTrue();
  });
});
