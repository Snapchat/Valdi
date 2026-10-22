import { ElementRef } from 'valdi_core/src/ElementRef';

export type NativeTemplateElementName =
  | 'custom-view'
  | 'layout'
  | 'view'
  | 'label'
  | 'textfield'
  | 'textview'
  | 'image'
  | 'webview'
  | 'video'
  | 'scroll'
  | 'spinner'
  | 'blur'
  | 'shape'
  | 'animatedimage';

export interface IntegrationTestRenderContext {
  readonly caseId: string;
  readonly rootRef: ElementRef<any>;
  readonly targetRef: ElementRef<any>;
  record(message: string): void;
}

export interface IntegrationTestInteractionContext extends IntegrationTestRenderContext {
  waitForIdle(): Promise<void>;
}

export interface IntegrationTestCase {
  readonly id: string;
  readonly name: string;
  readonly description: string;
  readonly element: NativeTemplateElementName;
  readonly coverage?: readonly IntegrationTestAttributeCoverage[];
  readonly render: (context: IntegrationTestRenderContext) => void;
  readonly interact?: (context: IntegrationTestInteractionContext) => Promise<void>;
  readonly skipSnapshotOnPlatforms?: readonly string[];
  readonly skipSnapshotReason?: string;
  readonly expectedFailureOnPlatforms?: readonly string[];
  readonly expectedFailureReason?: string;
}

export type IntegrationTestCoverageKind = 'visual' | 'interaction' | 'node-output' | 'blocked-needs-host';

export interface IntegrationTestAttributeCoverage {
  readonly attributes: readonly string[];
  readonly kind: IntegrationTestCoverageKind;
}

export interface IntegrationTestCaseResult {
  id: string;
  name: string;
  description: string;
  element: NativeTemplateElementName;
  nodeOutput?: IntegrationTestRenderedNode;
  snapshotBase64: string;
  observations: string;
  status: 'passed' | 'failed';
  error?: string;
}

export interface IntegrationTestRenderedNode {
  tag: string;
  attributes: { [name: string]: string };
  children: IntegrationTestRenderedNode[];
}

export interface IntegrationTestResult {
  schemaVersion: 1;
  platform: string;
  generatedAt: string;
  cases: IntegrationTestCaseResult[];
}
