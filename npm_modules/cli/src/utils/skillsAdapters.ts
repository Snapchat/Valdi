import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import type { SkillMeta } from './skillsRegistry';

export interface SkillAdapter {
  name: string;
  detect(): boolean;
  /** Install a skill. resourceDir is the bundled skill directory (contains scripts/ etc). */
  install(skillName: string, content: string, meta: SkillMeta, resourceDir?: string): void;
  remove(skillName: string): void;
  listInstalled(): string[];
}

/** Recursively copy a directory. Skips __pycache__ and test files. */
function copyDirSync(src: string, dst: string): void {
  fs.mkdirSync(dst, { recursive: true });
  for (const entry of fs.readdirSync(src, { withFileTypes: true })) {
    if (entry.name.startsWith('test_') || entry.name.endsWith('.spec.ts') || entry.name === '__pycache__') {
      continue;
    }
    const srcEntry = path.join(src, entry.name);
    const dstEntry = path.join(dst, entry.name);
    if (entry.isDirectory()) {
      copyDirSync(srcEntry, dstEntry);
    } else {
      fs.copyFileSync(srcEntry, dstEntry);
    }
  }
}

/** Kept for backwards compatibility with cli-sc's index.ts (no-op). */
export const conflictingClaudePluginKeys: string[] = [];

function getClaudeSkillsDir(): string {
  return path.join(os.homedir(), '.claude', 'skills');
}

// Remove leftover artifacts from the old plugin/marketplace approach (<=1.3.4).
function cleanupLegacyPluginArtifacts(): void {
  const claudeDir = path.join(os.homedir(), '.claude');
  const valdiPluginDir = path.join(claudeDir, 'plugins', 'local', 'valdi');
  if (fs.existsSync(valdiPluginDir)) {
    fs.rmSync(valdiPluginDir, { recursive: true, force: true });
  }
  const valdiCacheDir = path.join(claudeDir, 'plugins', 'cache', 'local', 'valdi');
  if (fs.existsSync(valdiCacheDir)) {
    fs.rmSync(valdiCacheDir, { recursive: true, force: true });
  }

  const installedPath = path.join(claudeDir, 'plugins', 'installed_plugins.json');
  if (fs.existsSync(installedPath)) {
    try {
      const data = JSON.parse(fs.readFileSync(installedPath, 'utf8'));
      if (Array.isArray(data)) {
        const filtered = data.filter(
          (entry: { key?: string }) => !entry.key?.startsWith('valdi'),
        );
        if (filtered.length !== data.length) {
          fs.writeFileSync(installedPath, JSON.stringify(filtered, null, 2), 'utf8');
        }
      }
    } catch {
      // Corrupt file — leave it alone
    }
  }

  const settingsPath = path.join(claudeDir, 'settings.json');
  if (fs.existsSync(settingsPath)) {
    try {
      const settings = JSON.parse(fs.readFileSync(settingsPath, 'utf8'));
      let changed = false;
      if (Array.isArray(settings.enabledPlugins)) {
        const before = settings.enabledPlugins.length;
        settings.enabledPlugins = settings.enabledPlugins.filter(
          (p: string) => !p.includes('valdi'),
        );
        if (settings.enabledPlugins.length === 0) delete settings.enabledPlugins;
        if ((settings.enabledPlugins?.length ?? 0) !== before) changed = true;
      }
      if (Array.isArray(settings.extraKnownMarketplaces)) {
        const before = settings.extraKnownMarketplaces.length;
        settings.extraKnownMarketplaces = settings.extraKnownMarketplaces.filter(
          (m: { name?: string }) => m.name !== 'local',
        );
        if (settings.extraKnownMarketplaces.length === 0) delete settings.extraKnownMarketplaces;
        if ((settings.extraKnownMarketplaces?.length ?? 0) !== before) changed = true;
      }
      if (changed) {
        fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2), 'utf8');
      }
    } catch {
      // Corrupt file — leave it alone
    }
  }
}

const ClaudeCodeAdapter: SkillAdapter = {
  name: 'claude',
  detect() {
    const claudeDir = path.join(os.homedir(), '.claude');
    return fs.existsSync(claudeDir);
  },
  install(skillName: string, content: string, meta: SkillMeta, resourceDir?: string) {
    cleanupLegacyPluginArtifacts();
    const skillDir = path.join(getClaudeSkillsDir(), skillName);
    fs.mkdirSync(skillDir, { recursive: true });
    const frontmatter = `---\nname: ${meta.name}\ndescription: ${meta.description}\n---\n\n`;
    fs.writeFileSync(path.join(skillDir, 'SKILL.md'), frontmatter + content, 'utf8');

    if (resourceDir) {
      for (const entry of fs.readdirSync(resourceDir, { withFileTypes: true })) {
        if (entry.isDirectory()) {
          copyDirSync(path.join(resourceDir, entry.name), path.join(skillDir, entry.name));
        }
      }
    }
  },
  remove(skillName: string) {
    const skillDir = path.join(getClaudeSkillsDir(), skillName);
    if (fs.existsSync(skillDir)) {
      fs.rmSync(skillDir, { recursive: true, force: true });
    }
  },
  listInstalled() {
    const skillsDir = getClaudeSkillsDir();
    if (!fs.existsSync(skillsDir)) return [];
    return fs
      .readdirSync(skillsDir)
      .filter((entry) => fs.statSync(path.join(skillsDir, entry)).isDirectory());
  },
};

// CursorAdapter: installs to ~/.cursor/skills/<name>/SKILL.md (global)
const CursorAdapter: SkillAdapter = {
  name: 'cursor',
  detect() {
    const cursorDir = path.join(os.homedir(), '.cursor');
    return fs.existsSync(cursorDir);
  },
  install(skillName: string, content: string, meta: SkillMeta, resourceDir?: string) {
    const skillDir = path.join(os.homedir(), '.cursor', 'skills', skillName);
    fs.mkdirSync(skillDir, { recursive: true });
    const frontmatter = `---\nname: ${meta.name}\ndescription: ${meta.description}\n---\n\n`;
    fs.writeFileSync(path.join(skillDir, 'SKILL.md'), frontmatter + content, 'utf8');

    if (resourceDir) {
      for (const entry of fs.readdirSync(resourceDir, { withFileTypes: true })) {
        if (entry.isDirectory()) {
          copyDirSync(path.join(resourceDir, entry.name), path.join(skillDir, entry.name));
        }
      }
    }

    // Clean up deprecated .mdc file from ~/.cursor/rules/ if it exists
    // (old adapter double-prefixed: valdi-${skillName}.mdc)
    const legacyFile = path.join(os.homedir(), '.cursor', 'rules', `valdi-${skillName}.mdc`);
    if (fs.existsSync(legacyFile)) {
      fs.unlinkSync(legacyFile);
    }
  },
  remove(skillName: string) {
    const skillDir = path.join(os.homedir(), '.cursor', 'skills', skillName);
    if (fs.existsSync(skillDir)) {
      fs.rmSync(skillDir, { recursive: true, force: true });
    }
    // Also clean up deprecated location (old adapter double-prefixed)
    const legacyFile = path.join(os.homedir(), '.cursor', 'rules', `valdi-${skillName}.mdc`);
    if (fs.existsSync(legacyFile)) {
      fs.unlinkSync(legacyFile);
    }
  },
  listInstalled() {
    const installed = new Set<string>();

    const skillsDir = path.join(os.homedir(), '.cursor', 'skills');
    if (fs.existsSync(skillsDir)) {
      for (const entry of fs.readdirSync(skillsDir)) {
        if (fs.existsSync(path.join(skillsDir, entry, 'SKILL.md'))) {
          installed.add(entry);
        }
      }
    }

    // Legacy: old adapter wrote valdi-${skillName}.mdc (double-prefixed),
    // so stripping one valdi- recovers the registry name.
    const rulesDir = path.join(os.homedir(), '.cursor', 'rules');
    if (fs.existsSync(rulesDir)) {
      for (const f of fs.readdirSync(rulesDir)) {
        if (f.startsWith('valdi-') && f.endsWith('.mdc')) {
          installed.add(f.replace(/^valdi-/u, '').replace(/\.mdc$/u, ''));
        }
      }
    }

    return Array.from(installed);
  },
};

// CopilotAdapter: appends to ./.github/copilot-instructions.md in CWD (project-scoped)
const CopilotAdapter: SkillAdapter = {
  name: 'copilot',
  detect() {
    const githubDir = path.join(process.cwd(), '.github');
    return fs.existsSync(githubDir);
  },
  install(skillName: string, content: string, _meta: SkillMeta) {
    const githubDir = path.join(process.cwd(), '.github');
    fs.mkdirSync(githubDir, { recursive: true });
    const instructionsFile = path.join(githubDir, 'copilot-instructions.md');
    const section = `\n\n## ${skillName}\n\n${content}`;
    if (fs.existsSync(instructionsFile)) {
      fs.appendFileSync(instructionsFile, section, 'utf8');
    } else {
      fs.writeFileSync(instructionsFile, `# Copilot Instructions${section}`, 'utf8');
    }
  },
  remove(skillName: string) {
    const instructionsFile = path.join(process.cwd(), '.github', 'copilot-instructions.md');
    if (!fs.existsSync(instructionsFile)) return;
    const contents = fs.readFileSync(instructionsFile, 'utf8');
    // Remove section starting with ## <skillName> up to next ## or end of file
    const sectionRegex = new RegExp(
      `\\n\\n## ${skillName}\\n[\\s\\S]*?(?=\\n\\n## |$)`,
      'u',
    );
    const updated = contents.replace(sectionRegex, '');
    fs.writeFileSync(instructionsFile, updated, 'utf8');
  },
  listInstalled() {
    const instructionsFile = path.join(process.cwd(), '.github', 'copilot-instructions.md');
    if (!fs.existsSync(instructionsFile)) return [];
    const contents = fs.readFileSync(instructionsFile, 'utf8');
    const matches = contents.match(/^## (valdi-\S+)/gmu);
    if (!matches) return [];
    return matches.map((m) => m.replace(/^## /u, ''));
  },
};

// GenericAdapter: installs to ~/.valdi/skills/<name>.md
const GenericAdapter: SkillAdapter = {
  name: 'generic',
  detect() {
    // Always available as a fallback
    return true;
  },
  install(skillName: string, content: string, _meta: SkillMeta) {
    const skillsDir = path.join(os.homedir(), '.valdi', 'skills');
    fs.mkdirSync(skillsDir, { recursive: true });
    fs.writeFileSync(path.join(skillsDir, `${skillName}.md`), content, 'utf8');
  },
  remove(skillName: string) {
    const filePath = path.join(os.homedir(), '.valdi', 'skills', `${skillName}.md`);
    if (fs.existsSync(filePath)) {
      fs.unlinkSync(filePath);
    }
  },
  listInstalled() {
    const skillsDir = path.join(os.homedir(), '.valdi', 'skills');
    if (!fs.existsSync(skillsDir)) return [];
    return fs
      .readdirSync(skillsDir)
      .filter((f) => f.endsWith('.md'))
      .map((f) => f.replace(/\.md$/u, ''));
  },
};

export const ALL_ADAPTERS: SkillAdapter[] = [
  ClaudeCodeAdapter,
  CursorAdapter,
  CopilotAdapter,
  GenericAdapter,
];

export function detectAdapters(): SkillAdapter[] {
  return ALL_ADAPTERS.filter((a) => a.detect());
}

export function getAdapterByName(name: string): SkillAdapter | undefined {
  return ALL_ADAPTERS.find((a) => a.name === name);
}
