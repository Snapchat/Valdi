import { createCachingWorkspace } from './cache/CachingWorkspaceFactory';
import { IWorkspace } from './IWorkspace';
import { ILogger } from './logger/ILogger';
import { Workspace } from './Workspace';

export interface CreateWorkspaceResult {
  workspaceId: number;
  workspace: IWorkspace;
}

interface StoreEntry {
  uncachedWorkspace: Workspace;
  activeWorkspace: IWorkspace;
}

export class WorkspaceStore {
  private store = new Map<number, StoreEntry>();
  private sequence = 0;

  constructor(
    readonly logger: ILogger,
    readonly cacheDir: string | undefined,
    readonly shouldDebounceOpenFile: boolean,
  ) {}

  createWorkspace(nativeApiMinVersion: number | undefined): CreateWorkspaceResult {
    if (
      nativeApiMinVersion !== undefined &&
      (!Number.isInteger(nativeApiMinVersion) || nativeApiMinVersion < 0 || nativeApiMinVersion > 2147483647)
    ) {
      throw new Error('nativeApiMinVersion must be an integer between 0 and 2147483647');
    }

    const uncachedWorkspace = new Workspace(
      '/',
      this.shouldDebounceOpenFile,
      this.logger,
      undefined,
      nativeApiMinVersion,
    );
    let workspace: IWorkspace;
    if (this.cacheDir) {
      workspace = createCachingWorkspace(this.cacheDir, uncachedWorkspace, this.logger, nativeApiMinVersion);
    } else {
      workspace = uncachedWorkspace;
    }

    const workspaceId = ++this.sequence;
    this.store.set(workspaceId, { uncachedWorkspace, activeWorkspace: workspace });

    return { workspaceId, workspace };
  }

  destroyWorkspace(workspaceId: number): boolean {
    const entry = this.store.get(workspaceId);
    if (!entry) {
      return false;
    }
    this.store.delete(workspaceId);
    entry.activeWorkspace.destroy();
    return true;
  }

  destroyAllWorkspaces(): void {
    const allWorkspaceIds = [...this.store.keys()];
    allWorkspaceIds.forEach((workspaceId) => this.destroyWorkspace(workspaceId));
  }

  private getEntry(workspaceId: number): StoreEntry {
    if (workspaceId === undefined) {
      throw new Error('workspaceId was not provided');
    }

    const entry = this.store.get(workspaceId);
    if (!entry) {
      throw new Error(`No Workspace for id ${workspaceId}`);
    }
    return entry;
  }

  getWorkspace(workspaceId: number): IWorkspace {
    return this.getEntry(workspaceId).activeWorkspace;
  }

  getInitializedWorkspace(workspaceId: number): IWorkspace {
    const workspace = this.getWorkspace(workspaceId);
    if (!workspace.initialized) {
      throw new Error(`Workspace ${workspaceId} was not initialized`);
    }
    return workspace;
  }

  getUncachedWorkspace(workspaceId: number): Workspace {
    return this.getEntry(workspaceId).uncachedWorkspace;
  }
}
