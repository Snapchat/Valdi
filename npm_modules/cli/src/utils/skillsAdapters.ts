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

const CLAUDE_PLUGIN_NAME = 'valdi@local';
const CLAUDE_PLUGIN_VERSION = '1.0.0';

function getClaudePluginInstallPath(): string {
  return path.join(
    os.homedir(),
    '.claude',
    'plugins',
    'cache',
    'local',
    'valdi',
    CLAUDE_PLUGIN_VERSION,
  );
}

function ensureClaudePluginRegistered(): void {
  const pluginsFile = path.join(os.homedir(), '.claude', 'plugins', 'installed_plugins.json');
  const installPath = getClaudePluginInstallPath();

  let data: { version: number; plugins: Record<string, unknown[]> } = { version: 2, plugins: {} };
  if (fs.existsSync(pluginsFile)) {
    try {
      const parsed = JSON.parse(fs.readFileSync(pluginsFile, 'utf8'));
      if (parsed && parsed.plugins) data = parsed;
    } catch {
      // Start fresh if file is corrupted
    }
  }

  const now = new Date().toISOString();
  const entry = {
    scope: 'user',
    installPath,
    version: CLAUDE_PLUGIN_VERSION,
    installedAt: now,
    lastUpdated: now,
  };

  const existing = data.plugins[CLAUDE_PLUGIN_NAME] as Array<{ installPath: string; lastUpdated: string }> | undefined;
  if (existing && existing.length > 0) {
    existing[0]!.installPath = installPath;
    existing[0]!.lastUpdated = now;
  } else {
    data.plugins[CLAUDE_PLUGIN_NAME] = [entry];
  }

  fs.mkdirSync(path.dirname(pluginsFile), { recursive: true });
  fs.writeFileSync(pluginsFile, JSON.stringify(data, null, 4), 'utf8');
}

// ClaudeCodeAdapter: installs to ~/.claude/plugins/cache/local/valdi/<version>/skills/<name>/SKILL.md
// and registers the plugin in ~/.claude/plugins/installed_plugins.json
const ClaudeCodeAdapter: SkillAdapter = {
  name: 'claude',
  detect() {
    const claudeDir = path.join(os.homedir(), '.claude');
    return fs.existsSync(claudeDir);
  },
  install(skillName: string, content: string, meta: SkillMeta, resourceDir?: string) {
    const installPath = getClaudePluginInstallPath();
    const skillDir = path.join(installPath, 'skills', skillName);
    fs.mkdirSync(skillDir, { recursive: true });
    const frontmatter = `---\nname: ${meta.name}\ndescription: ${meta.description}\n---\n\n`;
    fs.writeFileSync(path.join(skillDir, 'SKILL.md'), frontmatter + content, 'utf8');

    // Copy resource subdirectories (e.g. scripts/) so <skill_dir>/scripts/diff.py works
    if (resourceDir) {
      for (const entry of fs.readdirSync(resourceDir, { withFileTypes: true })) {
        if (entry.isDirectory()) {
          copyDirSync(path.join(resourceDir, entry.name), path.join(skillDir, entry.name));
        }
      }
    }

    ensureClaudePluginRegistered();
  },
  remove(skillName: string) {
    const installPath = getClaudePluginInstallPath();
    const skillDir = path.join(installPath, 'skills', skillName);
    if (fs.existsSync(skillDir)) {
      fs.rmSync(skillDir, { recursive: true, force: true });
    }
  },
  listInstalled() {
    const installPath = getClaudePluginInstallPath();
    const skillsDir = path.join(installPath, 'skills');
    if (!fs.existsSync(skillsDir)) return [];
    return fs
      .readdirSync(skillsDir)
      .filter((entry) => fs.statSync(path.join(skillsDir, entry)).isDirectory());
  },
};

// CursorAdapter: installs to ~/.cursor/rules/valdi-<name>.mdc (global)
const CursorAdapter: SkillAdapter = {
  name: 'cursor',
  detect() {
    const cursorDir = path.join(os.homedir(), '.cursor');
    return fs.existsSync(cursorDir);
  },
  install(skillName: string, content: string, meta: SkillMeta) {
    const rulesDir = path.join(os.homedir(), '.cursor', 'rules');
    fs.mkdirSync(rulesDir, { recursive: true });
    const frontmatter = `---\ndescription: ${meta.description}\nalwaysApply: false\n---\n\n`;
    fs.writeFileSync(path.join(rulesDir, `valdi-${skillName}.mdc`), frontmatter + content, 'utf8');
  },
  remove(skillName: string) {
    const filePath = path.join(os.homedir(), '.cursor', 'rules', `valdi-${skillName}.mdc`);
    if (fs.existsSync(filePath)) {
      fs.unlinkSync(filePath);
    }
  },
  listInstalled() {
    const rulesDir = path.join(os.homedir(), '.cursor', 'rules');
    if (!fs.existsSync(rulesDir)) return [];
    return fs
      .readdirSync(rulesDir)
      .filter((f) => f.startsWith('valdi-') && f.endsWith('.mdc'))
      .map((f) => f.replace(/^valdi-/u, '').replace(/\.mdc$/u, ''));
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
