import 'jasmine';
import { execSync } from 'child_process';
import path from 'path';

/**
 * Verifies that all directories required at runtime are included in the
 * published npm package. This prevents regressions like
 * https://github.com/Snapchat/Valdi/issues/93 where `valdi bootstrap` failed
 * because template files were missing from the tarball.
 */
describe('npm package contents', () => {
  let packedFiles: string;

  beforeAll(() => {
    const cliRoot = path.join(__dirname, '../..');
    packedFiles = execSync('npm pack --dry-run 2>&1', {
      cwd: cliRoot,
      encoding: 'utf-8',
    });
  });

  it('includes .metadata templates for bootstrap config', () => {
    expect(packedFiles).toContain('.metadata/config.yaml.template');
    expect(packedFiles).toContain('.metadata/WORKSPACE.template');
  });

  it('includes .bootstrap templates for project scaffolding', () => {
    expect(packedFiles).toContain('.bootstrap/');
  });

  it('includes dist output', () => {
    expect(packedFiles).toContain('dist/');
  });

  it('includes bundled-skills', () => {
    expect(packedFiles).toContain('bundled-skills/');
  });
});
