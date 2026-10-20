import fs from 'node:fs';
import path from 'node:path';
import { startStaticWebServer } from './webServerUtils';
import { decompressTo } from './zipUtils';

export interface WebApplicationSessionOptions {
  archivePath: string;
  host: string;
  liveReload?: boolean | undefined;
  port: number;
  workingDir: string;
}

export interface WebApplicationSession {
  url: string;
  close(): Promise<void>;
  publish(): Promise<void>;
}

async function extractWebSite(archivePath: string, sitesDir: string, generation: number): Promise<string> {
  const siteDir = path.join(sitesDir, String(generation));
  fs.mkdirSync(siteDir);
  try {
    await decompressTo(archivePath, siteDir);
    return siteDir;
  } catch (error) {
    fs.rmSync(siteDir, { force: true, recursive: true });
    throw error;
  }
}

export async function startWebApplicationSession(
  options: WebApplicationSessionOptions,
): Promise<WebApplicationSession> {
  const sitesDir = path.join(options.workingDir, 'sites');
  fs.mkdirSync(sitesDir);
  let generation = 1;
  let activeSiteDir = await extractWebSite(options.archivePath, sitesDir, generation);
  const staleSiteDirs: string[] = [];
  const server = await startStaticWebServer(activeSiteDir, {
    host: options.host,
    liveReload: options.liveReload ?? false,
    port: options.port,
  });

  let publicationQueue = Promise.resolve();
  const publish = async () => {
    const nextSiteDir = await extractWebSite(options.archivePath, sitesDir, ++generation);
    const previousSiteDir = activeSiteDir;
    activeSiteDir = nextSiteDir;
    server.setRootDir(nextSiteDir);
    server.reload();

    // Keep the immediately previous generation alive while in-flight responses finish.
    staleSiteDirs.push(previousSiteDir);
    while (staleSiteDirs.length > 1) {
      const staleSiteDir = staleSiteDirs.shift();
      if (staleSiteDir) {
        fs.rmSync(staleSiteDir, { force: true, recursive: true });
      }
    }
  };

  return {
    url: server.url,
    close: async () => {
      await publicationQueue.catch(() => {});
      await server.close();
    },
    publish: () => {
      publicationQueue = publicationQueue.then(publish, publish);
      return publicationQueue;
    },
  };
}
