import 'jasmine';
import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import { getAdapterByName, conflictingClaudePluginKeys } from './skillsAdapters';

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

  it('writes plugin.json manifest on install', () => {
    const adapter = getAdapter();
    adapter.install('test-skill', '# Test content', { name: 'test-skill', description: 'A test skill', tags: [], path: '', category: [] });

    const manifestPath = path.join(
      tmpHome, '.claude', 'plugins', 'cache', 'local', 'valdi', '1.0.0',
      '.claude-plugin', 'plugin.json',
    );
    expect(fs.existsSync(manifestPath)).toBe(true);

    const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    expect(manifest.name).toBe('valdi');
    expect(manifest.version).toBe('1.0.0');
    expect(manifest.description).toBeDefined();
  });

  it('writes SKILL.md with frontmatter', () => {
    const adapter = getAdapter();
    adapter.install('my-skill', '# My content', { name: 'my-skill', description: 'Desc', tags: [], path: '', category: [] });

    const skillPath = path.join(
      tmpHome, '.claude', 'plugins', 'cache', 'local', 'valdi', '1.0.0',
      'skills', 'my-skill', 'SKILL.md',
    );
    expect(fs.existsSync(skillPath)).toBe(true);

    const content = fs.readFileSync(skillPath, 'utf8');
    expect(content).toContain('name: my-skill');
    expect(content).toContain('description: Desc');
    expect(content).toContain('# My content');
  });

  it('registers plugin in installed_plugins.json', () => {
    const adapter = getAdapter();
    adapter.install('test-skill', '# Content', { name: 'test-skill', description: 'Desc', tags: [], path: '', category: [] });

    const pluginsFile = path.join(tmpHome, '.claude', 'plugins', 'installed_plugins.json');
    expect(fs.existsSync(pluginsFile)).toBe(true);

    const data = JSON.parse(fs.readFileSync(pluginsFile, 'utf8'));
    expect(data.plugins['valdi@local']).toBeDefined();
    expect(data.plugins['valdi@local'][0].installPath).toContain('cache/local/valdi/1.0.0');
  });

  it('lists installed skills', () => {
    const adapter = getAdapter();
    adapter.install('skill-a', '# A', { name: 'skill-a', description: 'A', tags: [], path: '', category: [] });
    adapter.install('skill-b', '# B', { name: 'skill-b', description: 'B', tags: [], path: '', category: [] });

    const installed = adapter.listInstalled();
    expect(installed).toContain('skill-a');
    expect(installed).toContain('skill-b');
  });

  it('removes conflicting plugin keys during install', () => {
    conflictingClaudePluginKeys.push('other-valdi@some-marketplace');

    const pluginsFile = path.join(tmpHome, '.claude', 'plugins', 'installed_plugins.json');
    fs.mkdirSync(path.dirname(pluginsFile), { recursive: true });
    fs.writeFileSync(pluginsFile, JSON.stringify({
      version: 2,
      plugins: {
        'other-valdi@some-marketplace': [{ scope: 'user', installPath: '/old/path', version: '1.0.0' }],
      },
    }), 'utf8');

    const adapter = getAdapter();
    adapter.install('test-skill', '# Content', { name: 'test-skill', description: 'Desc', tags: [], path: '', category: [] });

    const data = JSON.parse(fs.readFileSync(pluginsFile, 'utf8'));
    expect(data.plugins['other-valdi@some-marketplace']).toBeUndefined();
    expect(data.plugins['valdi@local']).toBeDefined();

    conflictingClaudePluginKeys.pop();
  });

  it('removes a skill', () => {
    const adapter = getAdapter();
    adapter.install('to-remove', '# Remove me', { name: 'to-remove', description: 'R', tags: [], path: '', category: [] });
    expect(adapter.listInstalled()).toContain('to-remove');

    adapter.remove('to-remove');
    expect(adapter.listInstalled()).not.toContain('to-remove');
  });
});
