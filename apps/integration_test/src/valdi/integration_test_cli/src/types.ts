export type Platform = 'android' | 'ios' | 'macos' | 'web';
export type CaseStatus = 'passed' | 'failed';

export interface IntegrationTestCaseResult {
  id: string;
  name: string;
  description?: string;
  element: string;
  nodeOutput?: IntegrationTestRenderedNode;
  snapshotBase64: string;
  observations?: string;
  status: CaseStatus;
  error?: string;
}

export interface IntegrationTestRenderedNode {
  tag: string;
  attributes: { [name: string]: string };
  children: IntegrationTestRenderedNode[];
}

export interface IntegrationTestResult {
  schemaVersion: number;
  platform: string;
  generatedAt: string;
  cases: IntegrationTestCaseResult[];
}
