import { fs } from 'file_system/src/FileSystem';

const TAG = '[FailedTestRetryReporter]';

export class FailedTestRetryReporter implements jasmine.CustomReporter {
  private readonly failedSpecs: jasmine.SpecResult[] = [];

  private existingReport?: { results: jasmine.SpecResult[] };

  constructor(private readonly path: string, private readonly logger?: (...params: unknown[]) => void) {
    this.readExistingReportIfPresent();
  }

  private log(...params: unknown[]) {
    this.logger?.(TAG, ...params);
  }

  specDone(result: jasmine.SpecResult, done?: () => void): void | Promise<void> {
    if (result.status === 'failed') {
      this.failedSpecs.push(result);
    }
    done?.();
  }

  jasmineDone(runDetails: jasmine.JasmineDoneInfo, done?: () => void): void | Promise<void> {
    this.log('Writing failed tests to', this.path, this.failedSpecs.length);
    fs.writeFileSync(this.path, JSON.stringify({ results: this.failedSpecs }, null, ' '));
    done?.();
  }

  // Filters out tests that are not listed in the failed tests report
  specFilter: jasmine.SpecFilter = (spec: jasmine.Spec) => {
    if (!this.existingReport) {
      // if there is no existing report, run all tests
      return true;
    }

    // if there is an existing report, only run tests that are listed as failures
    const previouslyFailed = this.existingReport.results.find(failedSpec => {
      return failedSpec.fullName === spec.getFullName();
    });

    if (previouslyFailed == undefined) {
      this.log('Skipping test, not present in failures', spec.getFullName());
      return false;
    }

    return true;
  };

  private readStringContents(content: string | ArrayBuffer): string {
    if (typeof content === 'string') {
      return content;
    } else {
      return new TextDecoder('utf-8').decode(content);
    }
  }

  private readExistingReportIfPresent() {
    try {
      const contents = this.readStringContents(fs.readFileSync(this.path, { encoding: 'utf8' }));
      this.existingReport = JSON.parse(contents);
    } catch (error) {
      // Failed to read report
      this.log('No existing failed test report, running all tests', this.path, error);
      this.existingReport = undefined;
    }
  }
}
