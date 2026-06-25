import 'jasmine';
import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import { getAdapterByName } from './skillsAdapters';

describe('ClaudeCodeAdapter', () => {
  let tmpHome: string;
  let origHome: string;

  beforeEach(() => {
    tmpHome = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-test-'));
    origHome = process.env['HOME']!;
    process.env['HOME'] = tmpHome;
    fs.mkdirSync(path.join(tmpHome, '.claude'), { recursive: true });
  });

  afterEach(() => {
    process.env['HOME'] = origHome;
    fs.rmSync(tmpHome, { recursive: true, force: true });
  });

  function getAdapter() {
    const adapter = getAdapterByName('claude');
    expect(adapter).toBeDefined();
    return adapter!;
  }

  it('writes SKILL.md with frontmatter to ~/.claude/skills/', () => {
    const adapter = getAdapter();
    adapter.install('my-skill', '# My content', { name: 'my-skill', description: 'Desc', tags: [], path: '', category: [] });

    const skillPath = path.join(tmpHome, '.claude', 'skills', 'my-skill', 'SKILL.md');
    expect(fs.existsSync(skillPath)).toBe(true);

    const content = fs.readFileSync(skillPath, 'utf8');
    expect(content).toContain('name: my-skill');
    expect(content).toContain('description: Desc');
    expect(content).toContain('# My content');
  });

  it('does not modify settings.json', () => {
    const settingsFile = path.join(tmpHome, '.claude', 'settings.json');
    fs.writeFileSync(settingsFile, JSON.stringify({ model: 'claude-opus-4-6' }), 'utf8');

    const adapter = getAdapter();
    adapter.install('test-skill', '# Content', { name: 'test-skill', description: 'Desc', tags: [], path: '', category: [] });

    const settings = JSON.parse(fs.readFileSync(settingsFile, 'utf8'));
    expect(settings).toEqual({ model: 'claude-opus-4-6' });
  });

  it('lists installed skills', () => {
    const adapter = getAdapter();
    adapter.install('skill-a', '# A', { name: 'skill-a', description: 'A', tags: [], path: '', category: [] });
    adapter.install('skill-b', '# B', { name: 'skill-b', description: 'B', tags: [], path: '', category: [] });

    const installed = adapter.listInstalled();
    expect(installed).toContain('skill-a');
    expect(installed).toContain('skill-b');
  });

  it('removes a skill', () => {
    const adapter = getAdapter();
    adapter.install('to-remove', '# Remove me', { name: 'to-remove', description: 'R', tags: [], path: '', category: [] });
    expect(adapter.listInstalled()).toContain('to-remove');

    adapter.remove('to-remove');
    expect(adapter.listInstalled()).not.toContain('to-remove');
  });
});

describe('CursorAdapter', () => {
  let tmpHome: string;
  let origHome: string;

  beforeEach(() => {
    tmpHome = fs.mkdtempSync(path.join(os.tmpdir(), 'valdi-test-'));
    origHome = process.env['HOME']!;
    process.env['HOME'] = tmpHome;
    fs.mkdirSync(path.join(tmpHome, '.cursor'), { recursive: true });
  });

  afterEach(() => {
    process.env['HOME'] = origHome;
    fs.rmSync(tmpHome, { recursive: true, force: true });
  });

  function getAdapter() {
    const adapter = getAdapterByName('cursor');
    expect(adapter).toBeDefined();
    return adapter!;
  }

  it('installs SKILL.md to ~/.cursor/skills/ without double-prefixing', () => {
    const adapter = getAdapter();
    adapter.install('valdi-bazel', '# Content', { name: 'valdi-bazel', description: 'A skill', tags: [], path: '', category: [] });

    const skillPath = path.join(tmpHome, '.cursor', 'skills', 'valdi-bazel', 'SKILL.md');
    expect(fs.existsSync(skillPath)).toBe(true);

    const content = fs.readFileSync(skillPath, 'utf8');
    expect(content).toContain('name: valdi-bazel');
    expect(content).toContain('description: A skill');
    expect(content).toContain('# Content');

    // Must NOT double-prefix
    const badPath = path.join(tmpHome, '.cursor', 'skills', 'valdi-valdi-bazel', 'SKILL.md');
    expect(fs.existsSync(badPath)).toBe(false);
  });

  it('does not install to deprecated ~/.cursor/rules/', () => {
    const adapter = getAdapter();
    adapter.install('valdi-bazel', '# Content', { name: 'valdi-bazel', description: 'A skill', tags: [], path: '', category: [] });

    const legacyPath = path.join(tmpHome, '.cursor', 'rules', 'valdi-valdi-bazel.mdc');
    expect(fs.existsSync(legacyPath)).toBe(false);
  });

  it('cleans up double-prefixed legacy .mdc file on install', () => {
    const legacyDir = path.join(tmpHome, '.cursor', 'rules');
    fs.mkdirSync(legacyDir, { recursive: true });
    fs.writeFileSync(path.join(legacyDir, 'valdi-valdi-bazel.mdc'), 'old content', 'utf8');

    const adapter = getAdapter();
    adapter.install('valdi-bazel', '# New content', { name: 'valdi-bazel', description: 'A skill', tags: [], path: '', category: [] });

    expect(fs.existsSync(path.join(legacyDir, 'valdi-valdi-bazel.mdc'))).toBe(false);
    expect(fs.existsSync(path.join(tmpHome, '.cursor', 'skills', 'valdi-bazel', 'SKILL.md'))).toBe(true);
  });

  it('lists installed skills', () => {
    const adapter = getAdapter();
    adapter.install('valdi-tsx', '# A', { name: 'valdi-tsx', description: 'A', tags: [], path: '', category: [] });
    adapter.install('valdi-bazel', '# B', { name: 'valdi-bazel', description: 'B', tags: [], path: '', category: [] });

    const installed = adapter.listInstalled();
    expect(installed).toContain('valdi-tsx');
    expect(installed).toContain('valdi-bazel');
  });

  it('lists legacy .mdc skills from ~/.cursor/rules/', () => {
    const rulesDir = path.join(tmpHome, '.cursor', 'rules');
    fs.mkdirSync(rulesDir, { recursive: true });
    // Old adapter wrote double-prefixed: valdi-${skillName}.mdc where skillName=valdi-legacy
    fs.writeFileSync(path.join(rulesDir, 'valdi-valdi-legacy.mdc'), 'old content', 'utf8');

    const adapter = getAdapter();
    expect(adapter.listInstalled()).toContain('valdi-legacy');
  });

  it('deduplicates skills present in both locations', () => {
    const adapter = getAdapter();
    adapter.install('valdi-both', '# New', { name: 'valdi-both', description: 'B', tags: [], path: '', category: [] });

    // Simulate leftover legacy file (double-prefixed)
    const rulesDir = path.join(tmpHome, '.cursor', 'rules');
    fs.mkdirSync(rulesDir, { recursive: true });
    fs.writeFileSync(path.join(rulesDir, 'valdi-valdi-both.mdc'), 'old', 'utf8');

    const installed = adapter.listInstalled();
    const count = installed.filter((s) => s === 'valdi-both').length;
    expect(count).toBe(1);
  });

  it('removes a skill and cleans up legacy file', () => {
    const adapter = getAdapter();
    adapter.install('valdi-to-remove', '# Remove', { name: 'valdi-to-remove', description: 'R', tags: [], path: '', category: [] });

    // Also create a double-prefixed legacy .mdc to verify cleanup
    const legacyDir = path.join(tmpHome, '.cursor', 'rules');
    fs.mkdirSync(legacyDir, { recursive: true });
    fs.writeFileSync(path.join(legacyDir, 'valdi-valdi-to-remove.mdc'), 'old', 'utf8');

    adapter.remove('valdi-to-remove');
    expect(adapter.listInstalled()).not.toContain('valdi-to-remove');
    expect(fs.existsSync(path.join(legacyDir, 'valdi-valdi-to-remove.mdc'))).toBe(false);
  });
});
