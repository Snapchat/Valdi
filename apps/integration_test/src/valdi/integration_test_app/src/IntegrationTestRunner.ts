import { ElementRef } from 'valdi_core/src/ElementRef';
import type { IRenderer } from 'valdi_core/src/IRenderer';
import { getValdiRuntime } from 'valdi_core/src/ValdiRuntimeProvider';
import { ViewNodeAssetTracker } from 'valdi_core/src/ViewNodeAssetTracker';
import { onIdleInterruptible } from 'valdi_core/src/utils/OnIdle';
import { wait } from 'valdi_core/src/utils/PromiseUtils';
import { fs } from 'file_system/src/FileSystem';

import { getOutputPath, getPlatform, markFinished, writeTextFile } from './IntegrationTestHost';
import { INTEGRATION_TEST_CASES } from './IntegrationTestCases';
import { toIntegrationTestRenderedNode } from './RenderedNodeOutput';
import {
  IntegrationTestCaseResult,
  IntegrationTestInteractionContext,
  IntegrationTestRenderedNode,
  IntegrationTestResult,
} from './IntegrationTestTypes';

const INTERACTION_TIMEOUT_MS = 3000;
const SNAPSHOT_TIMEOUT_MS = 5000;

export const SNAPSHOT_WIDTH = 360;
export const SNAPSHOT_HEIGHT = 560;

export interface IntegrationTestRefs {
  rootRef: ElementRef<any>;
  snapshotRef: ElementRef<any>;
  targetRef: ElementRef<any>;
}

export interface CaptureRequest extends IntegrationTestRefs {
  currentIndex: number;
  isFinished: boolean;
}

export type CaptureOutcome =
  | { kind: 'noop' }
  | { kind: 'advance'; nextIndex: number; summary: string }
  | { kind: 'finished'; summary: string };

export class IntegrationTestRunner {
  private readonly renderer: IRenderer;
  private readonly assetTracker = new ViewNodeAssetTracker();
  private readonly results: IntegrationTestCaseResult[] = [];
  private currentCaseId: string | undefined;
  private observations: string[] = [];
  private isCapturing = false;

  constructor(renderer: IRenderer) {
    this.renderer = renderer;
    getValdiRuntime().setViewNodeAssetTracker(renderer.contextId, this.assetTracker.onAssetEvent.bind(this.assetTracker));
  }

  record(message: string): void {
    this.observations.push(message);
  }

  prepareCase(caseId: string): void {
    if (this.currentCaseId === caseId) {
      return;
    }

    this.currentCaseId = caseId;
    this.observations = [];
  }

  private async withTimeout<T>(promise: Promise<T>, timeoutMs: number, label: string): Promise<T> {
    return await Promise.race([
      promise,
      wait(timeoutMs).then(() => {
        throw new Error(`${label} timed out after ${timeoutMs}ms`);
      }) as Promise<T>,
    ]);
  }

  private async waitForLayoutComplete(label: string): Promise<void> {
    await this.withTimeout(
      new Promise<void>(resolve => {
        this.renderer.onLayoutComplete(resolve);
      }),
      SNAPSHOT_TIMEOUT_MS,
      `${label} layout`,
    );
  }

  private async waitForIdle(label: string): Promise<void> {
    await this.withTimeout(
      new Promise<void>(resolve => {
        onIdleInterruptible(resolve);
      }),
      SNAPSHOT_TIMEOUT_MS,
      `${label} idle`,
    );
  }

  private async waitForSnapshotReadiness(label: string): Promise<void> {
    await this.waitForLayoutComplete(label);
    await this.waitForIdle(label);
  }

  private async waitForTrackedAssetLoads(caseId: string): Promise<void> {
    if (this.assetTracker.assetsCount === 0) {
      return;
    }

    await this.withTimeout(
      new Promise<void>(resolve => {
        this.assetTracker.onAllAssetsLoaded(resolve);
      }),
      SNAPSHOT_TIMEOUT_MS,
      `${caseId} asset load`,
    );
  }

  private async takeSnapshotWithRetry(refs: IntegrationTestRefs, caseId: string): Promise<string | undefined> {
    const attempts = 4;
    let lastError: any;

    for (let attempt = 0; attempt < attempts; attempt++) {
      if (attempt > 0) {
        await this.waitForSnapshotReadiness(`${caseId} snapshot attempt ${attempt + 1}`);
      }
      try {
        const root = refs.rootRef.single() ?? refs.snapshotRef.single();
        if (root) {
          const snapshot = await this.withTimeout(root.takeSnapshot(), SNAPSHOT_TIMEOUT_MS, `${caseId} snapshot attempt ${attempt + 1}`);
          if (snapshot) {
            return snapshot;
          }
        }
        this.record(`snapshot attempt ${attempt + 1}: no data`);
      } catch (error: any) {
        lastError = error;
        this.record(`snapshot attempt ${attempt + 1}: ${error?.message ?? String(error)}`);
      }
    }

    if (lastError && getPlatform() !== 'android') {
      throw lastError;
    }
    return undefined;
  }

  private captureRenderedNode(refs: IntegrationTestRefs): IntegrationTestRenderedNode | undefined {
    const element = refs.snapshotRef.single() ?? refs.rootRef.single();
    return element ? toIntegrationTestRenderedNode(element.getVirtualNode()) : undefined;
  }

  private writeProgress(currentIndex: number, phase: string, caseId?: string, error?: string): void {
    const outputPath = getOutputPath();
    const progress = JSON.stringify({
      phase,
      caseId,
      currentIndex,
      capturedCases: this.results.length,
      observations: this.observations.join('\n'),
      error,
      updatedAt: new Date().toISOString(),
    }, null, 2);

    try {
      writeTextFile(`${outputPath}.progress.json`, progress);
    } catch (_error) {
      // Native progress files are diagnostic only.
    }

    try {
      markFinished(`${outputPath}.progress.${phase}.${caseId ?? 'none'}`);
    } catch (_error) {
      // Native progress markers are diagnostic only.
    }

    try {
      this.ensureParentDirectory(outputPath);
      fs.writeFileSync(`${outputPath}.progress.json`, progress);
    } catch (_error) {
      // Progress files are diagnostic only; never fail the integration run.
    }
  }

  async captureCurrentCase(request: CaptureRequest): Promise<CaptureOutcome> {
    if (this.isCapturing || request.isFinished) {
      return { kind: 'noop' };
    }

    const testCase = INTEGRATION_TEST_CASES[request.currentIndex];
    if (!testCase) {
      return await this.finish(request.currentIndex);
    }

    this.prepareCase(testCase.id);
    this.isCapturing = true;
    this.writeProgress(request.currentIndex, 'started', testCase.id);

    try {
      const platform = getPlatform();
      await this.waitForIdle(`${testCase.id} render`);
      const context: IntegrationTestInteractionContext = {
        caseId: testCase.id,
        rootRef: request.rootRef,
        targetRef: request.targetRef,
        record: (message: string) => this.record(message),
        waitForIdle: () => this.waitForIdle(`${testCase.id} interaction`),
      };

      if (testCase.interact) {
        this.writeProgress(request.currentIndex, 'interacting', testCase.id);
        await this.withTimeout(testCase.interact(context), INTERACTION_TIMEOUT_MS, `${testCase.id} interaction`);
        await this.waitForSnapshotReadiness(`${testCase.id} post-interaction`);
      }

      await this.waitForTrackedAssetLoads(testCase.id);
      const nodeOutput = this.captureRenderedNode(request);

      if ((testCase.skipSnapshotOnPlatforms ?? []).indexOf(platform) !== -1) {
        const reason = testCase.skipSnapshotReason ?? `snapshot skipped on ${platform}`;
        this.record(reason);
        this.results.push({
          id: testCase.id,
          name: testCase.name,
          description: testCase.description,
          element: testCase.element,
          nodeOutput,
          snapshotBase64: '',
          observations: this.observations.join('\n'),
          status: 'failed',
          error: reason,
        });
        this.writeProgress(request.currentIndex, 'skipped', testCase.id, reason);
        this.isCapturing = false;
        return await this.advanceFromIndex(request.currentIndex);
      }

      this.writeProgress(request.currentIndex, 'snapshotting', testCase.id);
      const maybeSnapshot = await this.takeSnapshotWithRetry(request, testCase.id);
      const snapshotBase64 = maybeSnapshot ?? '';
      const expectedFailure = (testCase.expectedFailureOnPlatforms ?? []).indexOf(platform) !== -1;
      const expectedFailureReason = expectedFailure ? testCase.expectedFailureReason ?? `expected failure on ${platform}` : undefined;
      if (expectedFailureReason) {
        this.record(expectedFailureReason);
      }
      this.results.push({
        id: testCase.id,
        name: testCase.name,
        description: testCase.description,
        element: testCase.element,
        nodeOutput,
        snapshotBase64,
        observations: this.observations.join('\n'),
        status: snapshotBase64 && !expectedFailure ? 'passed' : 'failed',
        error: expectedFailureReason ?? (snapshotBase64 ? undefined : 'takeSnapshot returned no data'),
      });
      this.writeProgress(request.currentIndex, 'captured', testCase.id);
    } catch (error: any) {
      const message = error?.message ?? String(error);
      this.results.push({
        id: testCase.id,
        name: testCase.name,
        description: testCase.description,
        element: testCase.element,
        snapshotBase64: '',
        observations: this.observations.join('\n'),
        status: 'failed',
        error: message,
      });
      this.writeProgress(request.currentIndex, 'failed', testCase.id, message);
    }

    this.isCapturing = false;
    return await this.advanceFromIndex(request.currentIndex);
  }

  private async advanceFromIndex(currentIndex: number): Promise<CaptureOutcome> {
    const nextIndex = currentIndex + 1;
    if (nextIndex >= INTEGRATION_TEST_CASES.length) {
      return await this.finish(currentIndex);
    }

    return {
      kind: 'advance',
      nextIndex,
      summary: `captured ${nextIndex}/${INTEGRATION_TEST_CASES.length}`,
    };
  }

  private ensureParentDirectory(path: string): void {
    const slash = path.lastIndexOf('/');
    if (slash <= 0) {
      return;
    }
    const directory = path.slice(0, slash);
    try {
      fs.createDirectorySync(directory, true);
    } catch (_error) {
      // writeFileSync also creates parent directories in the native FS module.
      // Let the actual write report unwritable paths.
    }
  }

  private async finish(currentIndex: number): Promise<CaptureOutcome> {
    this.writeProgress(currentIndex, 'finishing');
    const outputPath = getOutputPath();
    const result: IntegrationTestResult = {
      schemaVersion: 1,
      platform: getPlatform(),
      generatedAt: new Date().toISOString(),
      cases: this.results,
    };

    const resultJson = JSON.stringify(result, null, 2);
    let wroteNativeResult = false;
    try {
      writeTextFile(outputPath, resultJson);
      writeTextFile(`${outputPath}.done`, 'done');
      wroteNativeResult = true;
    } catch (_error) {
      // Fall back to the shared JS FileSystem below.
    }
    try {
      this.ensureParentDirectory(outputPath);
      fs.writeFileSync(outputPath, resultJson);
      fs.writeFileSync(`${outputPath}.done`, 'done');
    } catch (error) {
      if (!wroteNativeResult) {
        throw error;
      }
    }
    markFinished(`${outputPath}.done`);
    console.log(`finished writing ${this.results.length} integration test case result(s) to ${outputPath}`);

    return {
      kind: 'finished',
      summary: `wrote ${this.results.length} cases to ${outputPath}`,
    };
  }
}
