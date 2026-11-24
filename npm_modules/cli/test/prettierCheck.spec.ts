import * as fs from 'fs';
import * as os from 'os';
import path from 'path';
import { runPrettierCheck } from '../src/utils/prettierCheck';

describe('runPrettierCheck', () => {
  let dir: string;

  beforeEach(() => {
    dir = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-prettier-'));
  });
  afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

  it('passes for formatted files', async () => {
    fs.writeFileSync(path.join(dir, 'a.ts'), 'export const a = 1;\n');
    expect(await runPrettierCheck('**/*.ts', dir)).toEqual({ status: 'ok' });
  });

  it('reports unformatted files as unformatted, not as an error', async () => {
    fs.writeFileSync(path.join(dir, 'a.ts'), 'export   const a=1\n');
    const result = await runPrettierCheck('**/*.ts', dir);
    expect(result.status).toBe('unformatted');
  });

  it('reports a formatter that cannot run as an error', async () => {
    const result = await runPrettierCheck('**/*.ts', dir, path.join(dir, 'missing-prettier.cjs'));
    expect(result.status).toBe('error');
  });

  it('does not need a project-local prettier or the network', async () => {
    fs.writeFileSync(path.join(dir, 'a.ts'), 'export const a = 1;\n');
    const result = await runPrettierCheck('**/*.ts', dir);
    expect(result.status).toBe('ok');
    expect(fs.existsSync(path.join(dir, 'node_modules'))).toBe(false);
  });
});
