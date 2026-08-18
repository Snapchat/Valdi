import 'jasmine/src/jasmine';
import { getModuleLoader } from 'valdi_core/src/ModuleLoaderGlobal';
import { symbolicateErrorStack } from 'valdi_core/src/Symbolicator';
import { UncaughtErrorHandlerResult, setCatchAllUncaughtErrorHandler } from 'valdi_core/src/UncaughtErrorHandler';
import { toError } from 'valdi_core/src/utils/ErrorUtils';
import { fs } from 'file_system/src/FileSystem';
import { getStandaloneRuntime } from 'valdi_standalone/src/ValdiStandalone';
import { CodeCoverageManager } from './CodeCoverage';
import { FailedTestRetryReporter } from './FailedTestRetryReporter';
import { ValdiGlobalTestSuiteDoneHandler } from 'valdi_core/src/utils/TestUtils';

const jasmineBootstrapGlobal = globalThis as typeof globalThis &
  ValdiGlobalTestSuiteDoneHandler & {
    __jasmineTestRunner__?: JasmineTestsRunnerImpl;
  };

type MutableJasmineBootstrapGlobal = {
  describe?: (description: string, specDefinitions: () => void) => void;
  xdescribe?: (description: string, specDefinitions: () => void) => void;
  jasmine?: any;
};

type TestLoadCallback = (onStartLoadingNewImportPath: (importPath: string) => void) => void;

interface PendingTests {
  showColors: boolean;
  iterationsCount: number;
  cb: TestLoadCallback;
  jasmineSeed?: number;
}

interface JasmineTestsRunner {
  scheduleTests(tests: PendingTests): void;
}

interface JasmineTestsRunnerConfig {
  showColors: boolean;
  allowIncompleteTestRun: boolean;
  testErrorAllowList: string[];
  iterationsCount?: number;
  codeCoverage?: boolean;
  codeCoverageResultFile?: string;
  junitReportOutputDir?: string;
  junitReportFilename?: string;
  strictErrorHandling?: boolean;
  retryFailures?: boolean;
  jasmineSeed?: number;
}

interface JasmineSpec {
  evalFn: () => void;
}

function processErrorStack(stack: string | undefined): string {
  if (!stack) {
    return '';
  }

  const filteredStack = stack
    .split('\n')
    .filter(n => n.indexOf('<Jasmine>') < 0)
    .join('\n');

  return symbolicateErrorStack(filteredStack);
}

// Used to associate a test suite with the import path of the test source file
// that contains it
const importPathSuitePropertyKey = 'importPathSuitePropertyKey';

class JasmineTestsRunnerImpl implements JasmineTestsRunner, jasmine.CustomReporter {
  private pendingTests: PendingTests[] = [];
  private runningTests = false;
  private testsLoadFailures: Error[] = [];
  private junitReporter: any = undefined;

  constructor(private config: JasmineTestsRunnerConfig, private codeCoverageManage: CodeCoverageManager | undefined) {}

  scheduleTests(tests: PendingTests): void {
    this.pendingTests.push(tests);
    if (!this.runningTests) {
      this.runNextTests();
    }
  }

  jasmineStarted(suiteInfo: jasmine.JasmineStartedInfo) {
    console.log(`- Jasmine execution started (${suiteInfo.totalSpecsDefined} specs)`);

    setCatchAllUncaughtErrorHandler((isHandledPromise, error) => {
      if (isHandledPromise) {
        this.onUnhandledRejection(error);
      } else {
        this.onUncaughtException(error);
      }
      return UncaughtErrorHandlerResult.IGNORE;
    });
  }
  suiteStarted(result: jasmine.SuiteResult) {
    console.log(`-------------------------------------`);
    console.log(`-- Suite started: ${result.fullName}`);
  }
  specStarted(result: jasmine.SuiteResult) {
    console.log(`---- Spec started: ${result.description}`);
  }
  specDone(result: jasmine.SuiteResult) {
    // not logging that a spec is done, since jasmine prints a character (without newline) when a spec is done
    // see jasmine/src/console_reporter.js this.specDone
  }
  suiteDone(result: jasmine.SuiteResult) {
    console.log(`-- Suite ${result.status}: ${result.fullName}`);
    console.log(`-------------------------------------`);

    if (result.status.includes('failed')) {
      const syntheticSpec = {
        id: `${result.id}-suite-failure`,
        description: 'Injected Dummy spec for suite-level error reporting',
        fullName: `${result.fullName} (suite-level failure)`,
        status: 'failed',
        failedExpectations: result.failedExpectations,
        duration: result.duration,
      };

      if (this.junitReporter !== undefined) {
        console.log('JUnit reporter is defined');
      }

      this.junitReporter?.specDone?.(syntheticSpec); // Log to results.xml via junit reporter
    }
    if (jasmineBootstrapGlobal.__onValdiTestSuiteDone) {
      jasmineBootstrapGlobal.__onValdiTestSuiteDone();
    }
  }

  private onUncaughtException(err: unknown) {
    console.error('Uncaught JS error:', err);
    this.notifyOnError(err);
  }

  private onUnhandledRejection(err: unknown) {
    console.error('Unhandled Promise rejection:', err);
    this.notifyOnError(err);
  }

  private isTestErrorAllowListed(error: Error): boolean {
    const stack = error.stack ?? '';

    for (const entry of this.config.testErrorAllowList) {
      if (stack.indexOf(entry) >= 0) {
        return true;
      }
    }

    return false;
  }

  private notifyOnError(err: unknown) {
    const error = toError(err);

    if (!this.config.strictErrorHandling && this.isTestErrorAllowListed(error)) {
      return;
    }

    if (typeof onerror != 'undefined' && onerror) {
      onerror(error as any, undefined, undefined, undefined, error);
    }
  }

  jasmineDone(result: jasmine.JasmineDoneInfo) {
    const testsFailedToLoad = this.testsLoadFailures.length > 0;
    console.log(`- Jasmine execution done: ${testsFailedToLoad ? 'tests failed to load' : result.overallStatus}`);
    setCatchAllUncaughtErrorHandler(undefined);

    const failed = testsFailedToLoad || result.overallStatus === 'failed' || result.failedExpectations.length;
    const incomplete = result && result.overallStatus === 'incomplete';

    const valdiStandalone = getStandaloneRuntime();
    if (this.codeCoverageManage) {
      try {
        this.codeCoverageManage.collectAndSaveCoverageInfo();
      } catch (err) {
        console.error('Failed to save code coverage info', err);
        if (!valdiStandalone.debuggerEnabled) {
          valdiStandalone.exit(1);
        }
      }
    }

    for (const testLoadFailure of this.testsLoadFailures) {
      this.logTestLoadFailure(testLoadFailure);
    }
    this.testsLoadFailures = [];
    this.runningTests = false;

    if (valdiStandalone.debuggerEnabled) {
      setTimeout(() => {
        this.runNextTests();
      });
      return;
    }

    if (incomplete && !this.config.allowIncompleteTestRun) {
      console.log(``);
      console.log(`============================================================`);
      console.log(`Test run INCOMPLETE: ${result.incompleteReason}`);
      console.log(``);
      console.log(`Have you accidentally checked in a test with fit() or fdescribe()?`);
      console.log(`============================================================`);
      console.log(``);
      valdiStandalone.exit(1);
      return;
    }

    if (failed) {
      console.log(``);
      console.log(`============================================================`);
      console.log(`Test run FAILED`);
      console.log(``);
      console.log(`Please review the above test results and log and fix any test failures`);
      console.log(`============================================================`);
      console.log(``);
      valdiStandalone.exit(1);
    } else {
      console.log(``);
      console.log(`============================================================`);
      console.log(`Test run COMPLETE!`);
      console.log(`============================================================`);
      valdiStandalone.exit(0);
    }
  }

  private logTestLoadFailure(error: Error) {
    console.log(`============================================================`);
    console.log(`Test FAILED to load`);
    console.log(``);
    console.log(error);
    if (error instanceof Error) {
      console.log(`Stack trace:`);
      console.log(processErrorStack(error.stack));
    }
    console.log(`============================================================`);
    console.log(``);
  }

  private loadTests(tests: PendingTests): JasmineSpec[] {
    const jasmineSpecs: JasmineSpec[] = [];

    let currentImportPath: string | undefined = undefined;
    const updateCurrentImportPath = (importPath: string) => {
      currentImportPath = importPath;
    };

    const jasmineDescribeFns = ['describe', 'xdescribe'] as const;
    const mutableJasmineBootstrapGlobal = globalThis as MutableJasmineBootstrapGlobal;
    for (const jasmineDescribeFn of jasmineDescribeFns) {
      mutableJasmineBootstrapGlobal[jasmineDescribeFn] = (description: string, specDefinitions: () => void) => {
        if (!currentImportPath) {
          throw new Error('Unknown current import path!');
        }
        const capturedImportPath = currentImportPath;
        jasmineSpecs.push({
          evalFn: () => {
            // inject the setSuiteProperty call to add the import path information
            const wrappedSpecDefinitions = () => {
              // Calling this in beforeAll ensures that the property is set on the root-most
              // spec in each evaluated file.
              beforeAll(() => {
                setSuiteProperty(importPathSuitePropertyKey, capturedImportPath);
              });
              afterEach(() => {
                // UXP-156: Some tests may not clean up after themselves when calling jasmine.clock().mock()
                // As a precaution we're uninstalling mocks so they don't leak into other tests/processes.
                jasmine.clock().uninstall();
              });
              specDefinitions();
            };
            mutableJasmineBootstrapGlobal[jasmineDescribeFn]!.apply(undefined, [
              description,
              wrappedSpecDefinitions,
            ]);
          },
        });
      };
      mutableJasmineBootstrapGlobal['jasmine'] = new Proxy(
        {},
        {
          get(target, property) {
            throw new Error(
              `jasmine.${String(property)}() cannot called outside of a describe() or xdescribe() context.`,
            );
          },
        },
      );
    }

    try {
      tests.cb(updateCurrentImportPath);
    } catch (error: any) {
      this.testsLoadFailures.push(error);
      this.logTestLoadFailure(error);
    } finally {
      for (const jasmineDescribeFn of jasmineDescribeFns) {
        mutableJasmineBootstrapGlobal[jasmineDescribeFn] = undefined;
      }
      mutableJasmineBootstrapGlobal['jasmine'] = undefined;
    }

    return jasmineSpecs;
  }

  // Hack JUnitXmlReporter by overriding its writeFile function
  // with an implementation that uses Valdi standalone runtime's file system
  // functions.
  private hackJunitXMLReporter(junitXmlReporterInstance: any) {
    if (!junitXmlReporterInstance.writeFile) {
      throw new Error('FATAL: We expect to be able to overwrite the writeFile function');
    }
    junitXmlReporterInstance.writeFile = (filename: string, text: string) => {
      const fullOutputPath = `${this.config.junitReportOutputDir}/${filename}`;
      fs.writeFileSync(fullOutputPath, text);
    };
  }

  getImportPathFromSuite = (suite: any): string | undefined => {
    return suite?.properties?.[importPathSuitePropertyKey];
  };

  // Delegate block used by junit_reporter, we use this to ensure that the
  // test suite name include the absolute import path of the file that contains it.
  //
  // Note: jasmine or junit_reporter silently stops when we throw an in this delegate block
  // so we explicitly catch and log an error message ourselves.
  modifyJUnitReporterSuiteName = (name: string, suite: any) => {
    try {
      let prefix = this.getImportPathFromSuite(suite) ?? '';
      let currentParent = suite._parent;
      while (currentParent) {
        const importPath = this.getImportPathFromSuite(currentParent);
        if (importPath) {
          prefix = `${importPath}${prefix}`;
        }
        currentParent = currentParent._parent;
      }

      const modified = `${prefix}#${name}`;
      return modified;
    } catch (error) {
      console.error(`FATAL ERROR when modifying suite name for junit test report:`, error);
      throw error;
    }
  };

  private runNextTests() {
    const tests = this.pendingTests.shift();
    if (!tests) {
      return;
    }

    const jasmineSpecs = this.loadTests(tests);

    this.runningTests = true;

    const Jasmine = getModuleLoader().load('jasmine/src/boot', true);

    const failedTestRetryReporter =
      this.config.retryFailures === true
        ? new FailedTestRetryReporter(
            `${this.config.junitReportOutputDir}/${this.config.junitReportFilename ?? 'test'}-failures.json`,
            (...params) => {
              console.log(...params);
            },
          )
        : null;

    const jasmineInstance = new Jasmine(
      tests.showColors,
      processErrorStack,
      tests.jasmineSeed,
      failedTestRetryReporter?.specFilter,
    );

    jasmineInstance.addReporter(this);

    if (failedTestRetryReporter) {
      jasmineInstance.addReporter(failedTestRetryReporter);
    }

    // No need to produce test report when running with the hot reloader
    const junitReportOutputDir = this.config;
    if (junitReportOutputDir) {
      const junit_reporter = getModuleLoader().load('jasmine/src/jasmine-reporters/junit_reporter', true);
      const junitXmlReporterInstance = new junit_reporter.JUnitXmlReporter({
        // we can't use the `savePath` config option because we're overriding the writeFile function
        consolidateAll: true, // output a single report file
        filePrefix: this.config.junitReportFilename, // `filePrefix` is used as the full file name when consolidateAll is true,
        modifySuiteName: this.modifyJUnitReporterSuiteName,
      });
      this.hackJunitXMLReporter(junitXmlReporterInstance);
      this.junitReporter = junitXmlReporterInstance;
      jasmineInstance.addReporter(junitXmlReporterInstance);
    }

    for (let i = 0; i < tests.iterationsCount; i++) {
      for (const spec of jasmineSpecs) {
        spec.evalFn();
      }
    }

    jasmineInstance.execute();
  }
}

export function prepareTests(config: JasmineTestsRunnerConfig, cb: TestLoadCallback) {
  let codeCoverageManager = undefined;

  if (config.codeCoverage && config.codeCoverageResultFile) {
    codeCoverageManager = new CodeCoverageManager(config.codeCoverageResultFile);
  }
  // Jasmine uses globals under the hood, so we have to workaround it and ensure
  // we don't set it up while tests are running
  let testsRunner = jasmineBootstrapGlobal.__jasmineTestRunner__;
  if (!testsRunner) {
    testsRunner = new JasmineTestsRunnerImpl(config, codeCoverageManager);
    jasmineBootstrapGlobal.__jasmineTestRunner__ = testsRunner;
  }

  testsRunner.scheduleTests({
    showColors: config.showColors,
    iterationsCount: config.iterationsCount ?? 1,
    jasmineSeed: config.jasmineSeed,
    cb,
  });
}
