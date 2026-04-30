import * as path from 'path';
import * as fs from 'fs';

export interface SkillMeta {
  name: string;
  description: string;
  tags: string[];
  path: string;
  /** 'framework' = for working on the Valdi repo itself (runtime, compiler, build rules)
   *  'client'    = for creating / updating / testing Valdi modules
   *  A skill may belong to both categories. */
  category: ('framework' | 'client')[];
}

export interface SkillRegistry {
  version: string;
  skills: SkillMeta[];
}

// Returns the bundled-skills/ directory shipped inside the npm package
// (sits next to dist/ after build).
function findBundledSkillsDir(): string | null {
  const candidate = path.resolve(__dirname, '..', '..', 'bundled-skills');
  return fs.existsSync(path.join(candidate, 'registry.json')) ? candidate : null;
}

// --- Public API ---

// Load registry from the bundled skills shipped in the npm package.
export function loadLocalRegistry(): SkillRegistry | null {
  const bundledDir = findBundledSkillsDir();
  if (bundledDir == null) return null;
  try {
    return JSON.parse(
      fs.readFileSync(path.join(bundledDir, 'registry.json'), 'utf8'),
    ) as SkillRegistry;
  } catch {
    return null;
  }
}

// Used by list/install/update — reads from the bundled skills, no network.
export async function fetchRegistry(): Promise<SkillRegistry> {
  const local = loadLocalRegistry();
  if (local != null) {
    return local;
  }
  throw new Error(
    'No bundled skills found. Re-install @snap/valdi to get the latest bundle.',
  );
}

// Read skill content from the bundled skills shipped in the npm package.
export async function fetchSkillContent(skillPath: string): Promise<string> {
  const bundledDir = findBundledSkillsDir();
  if (bundledDir != null) {
    const candidate = path.join(bundledDir, skillPath);
    if (fs.existsSync(candidate)) {
      return fs.readFileSync(candidate, 'utf8');
    }
  }
  throw new Error(
    `Skill content not found for "${skillPath}". Re-install @snap/valdi to get the latest bundle.`,
  );
}

// Return the bundled directory for a skill (e.g. bundled-skills/skills/valdi-mock-to-module/).
// Used by adapters that need to install resource files (scripts/) alongside the skill.
export function getSkillResourceDir(skillPath: string): string | null {
  const bundledDir = findBundledSkillsDir();
  if (bundledDir == null) return null;
  const dir = path.join(bundledDir, path.dirname(skillPath));
  return fs.existsSync(dir) ? dir : null;
}
