import { runCliCommand } from './cliUtils';

/**
 * Parses a GitHub repo URL (e.g. https://github.com/Snapchat/Valdi or .git variant)
 * into "owner/repo" for the GitHub API.
 */
function parseGitHubRepoSlug(repoUrl: string): string {
  const normalized = repoUrl.replace(/\.git$/u, '').trim();
  const match = normalized.match(/github\.com[/:]([^/]+)\/([^/]+?)(?:[/]?$)/u);
  if (!match) {
    throw new Error(`Invalid GitHub URL: ${repoUrl}`);
  }
  return `${match[1]}/${match[2]}`;
}

const LATEST_RELEASE_HINT =
  'Use --valdiVersion=latest to retry, or --valdiVersion=<tag> (e.g. main or v1.0.0) to pin a version.';

/**
 * Returns the tag name of the release marked as "Latest" on GitHub (releases UI).
 * Uses only the GitHub Releases API so the result matches what GitHub shows.
 * Falls back to git ls-remote only when the repo has no releases (404).
 */
export async function getLatestReleaseTag(repoUrl: string): Promise<string> {
  const slug = parseGitHubRepoSlug(repoUrl);
  let response: Response;
  try {
    response = await fetch(`https://api.github.com/repos/${slug}/releases/latest`, {
      headers: { 'User-Agent': 'Valdi-CLI' },
    });
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    throw new Error(
      `Could not fetch latest release from GitHub (${message}). ${LATEST_RELEASE_HINT}`,
    );
  }
  if (!response.ok) {
    if (response.status === 404) {
      // No releases or no "latest" release – fall back to git ls-remote
      return getLatestReleaseTagFromGit(repoUrl);
    }
    throw new Error(
      `GitHub API returned ${response.status} (rate limit or server error). ${LATEST_RELEASE_HINT}`,
    );
  }
  const data = (await response.json()) as { tag_name?: string };
  if (typeof data?.tag_name !== 'string') {
    throw new Error(`Invalid response: missing tag_name. ${LATEST_RELEASE_HINT}`);
  }
  return data.tag_name;
}

/** Matches semver-like tags (e.g. v1.0.0, 2.3.1, beta-0.0.1). Captures major, minor, patch. */
const SEMVER_TAG_REGEX = /^(?:\w+-)?v?(\d+)\.(\d+)\.(\d+)(?:[-.]\S+)?$/u;

function parseSemverTag(tag: string): [number, number, number] | null {
  const m = tag.match(SEMVER_TAG_REGEX);
  if (!m) return null;
  return [Number.parseInt(m[1]!, 10), Number.parseInt(m[2]!, 10), Number.parseInt(m[3]!, 10)];
}

function compareSemverTags(a: string, b: string): number {
  const va = parseSemverTag(a);
  const vb = parseSemverTag(b);
  if (va && vb) {
    return va[0] - vb[0] || va[1] - vb[1] || va[2] - vb[2];
  }
  if (va) return 1;   // non-semver (b) sorts before semver (a)
  if (vb) return -1;  // non-semver (a) sorts before semver (b)
  return a.localeCompare(b, undefined, { numeric: true });
}

/**
 * Fallback: resolve tag refs via git ls-remote and return the latest tag by semver.
 * git ls-remote returns refs sorted by refname (alphabetically), so we parse all tags
 * and pick the highest semantic version.
 */
async function getLatestReleaseTagFromGit(repoUrl: string): Promise<string> {
  const gitUrl = repoUrl.includes('.git') ? repoUrl : `${repoUrl}.git`;
  const result = await runCliCommand(`git ls-remote --tags --refs ${gitUrl}`, undefined, true);
  const lines = result.stdout.trim().split('\n').filter(Boolean);
  const tags: string[] = [];
  for (const line of lines) {
    const ref = line.split(/\s+/)[1];
    if (ref?.startsWith('refs/tags/')) {
      const tag = ref.slice('refs/tags/'.length);
      if (tag) tags.push(tag);
    }
  }
  if (tags.length === 0) {
    throw new Error('No tags found in repository');
  }
  tags.sort(compareSemverTags);
  return tags[tags.length - 1]!;
}

/** @deprecated Use getLatestReleaseTag for the GitHub "Latest" release; this returns an arbitrary tag from git ls-remote. */
export async function resolveLatestReleaseRef(repoUrl: string): Promise<string> {
  const tag = await getLatestReleaseTagFromGit(repoUrl);
  // Template expects either ref (refs/tags/X) or tag name for archive URL; we use tag name.
  return tag;
}
