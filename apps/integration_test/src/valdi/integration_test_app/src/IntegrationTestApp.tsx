import { StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { ElementRef } from 'valdi_core/src/ElementRef';

import { INTEGRATION_TEST_CASES } from './IntegrationTestCases';
import {
  CaptureOutcome,
  IntegrationTestRunner,
  SNAPSHOT_HEIGHT,
  SNAPSHOT_WIDTH,
} from './IntegrationTestRunner';

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

/**
 * @Context
 * @ExportModel
 */
export interface ComponentContext {}

interface State {
  currentIndex: number;
  finished: boolean;
  summary: string;
  insetTop: number;
  insetRight: number;
  insetBottom: number;
  insetLeft: number;
}

function readShellInsets(): Pick<State, 'insetTop' | 'insetRight' | 'insetBottom' | 'insetLeft'> {
  return {
    insetTop: Device.getDisplayTopInset(),
    insetRight: Device.getDisplayRightInset(),
    insetBottom: Device.getDisplayBottomInset(),
    insetLeft: Device.getDisplayLeftInset(),
  };
}

/**
 * @Component
 * @ExportModel
 */
export class IntegrationTestApp extends StatefulComponent<ViewModel, State, ComponentContext> {
  state: State = {
    currentIndex: 0,
    finished: false,
    summary: 'starting',
    ...readShellInsets(),
  };

  private rootRef = new ElementRef<any>();
  private snapshotRef = new ElementRef<any>();
  private targetRef = new ElementRef<any>();
  private runner?: IntegrationTestRunner;
  private hasStarted = false;
  private scheduledCaptureCaseId?: string;

  onCreate(): void {
    this.runner = new IntegrationTestRunner(this.renderer);

    const insetsObserver = Device.observeDisplayInsetChange(() => {
      this.setState(readShellInsets());
    });
    this.registerDisposable(() => insetsObserver.cancel());
  }

  private getRunner(): IntegrationTestRunner {
    if (!this.runner) {
      this.runner = new IntegrationTestRunner(this.renderer);
    }
    return this.runner;
  }

  private async captureCurrentCase(): Promise<void> {
    const runner = this.getRunner();
    this.hasStarted = true;
    const outcome = await runner.captureCurrentCase({
      currentIndex: this.state?.currentIndex ?? 0,
      isFinished: this.state?.finished ?? false,
      rootRef: this.rootRef,
      snapshotRef: this.snapshotRef,
      targetRef: this.targetRef,
    });
    this.applyCaptureOutcome(outcome);
  }

  private applyCaptureOutcome(outcome: CaptureOutcome): void {
    if (outcome.kind === 'noop') {
      return;
    }

    if (outcome.kind === 'finished') {
      this.setState({
        finished: true,
        summary: outcome.summary,
      });
      return;
    }

    this.rootRef = new ElementRef<any>();
    this.snapshotRef = new ElementRef<any>();
    this.targetRef = new ElementRef<any>();
    this.setState({
      currentIndex: outcome.nextIndex,
      summary: outcome.summary,
    });
  }

  private scheduleCaptureForRenderedCase(caseId: string): void {
    if (this.scheduledCaptureCaseId === caseId) {
      return;
    }

    this.scheduledCaptureCaseId = caseId;
    this.renderer.onLayoutComplete(() => {
      const currentIndex = this.state?.currentIndex ?? 0;
      const currentCase = INTEGRATION_TEST_CASES[currentIndex];
      if (!this.state?.finished && currentCase?.id === caseId) {
        void this.captureCurrentCase();
      }
    });
  }

  onRender(): void {
    const currentIndex = this.state?.currentIndex ?? 0;
    const testCase = INTEGRATION_TEST_CASES[currentIndex];
    const insetTop = this.state?.insetTop ?? 0;
    const insetRight = this.state?.insetRight ?? 0;
    const insetBottom = this.state?.insetBottom ?? 0;
    const insetLeft = this.state?.insetLeft ?? 0;

    if (this.state?.finished || !testCase) {
      <view
        width="100%"
        height="100%"
        backgroundColor="#F8FAFC"
        paddingTop={24 + insetTop}
        paddingRight={24 + insetRight}
        paddingBottom={24 + insetBottom}
        paddingLeft={24 + insetLeft}
      >
        <label value="Valdi integration tests finished" font="system-bold 22" color="#111827" marginBottom={16} />
        <label value={this.state?.summary ?? 'finished'} font="system 16" color="#334155" numberOfLines={0} />
      </view>;
      return;
    }

    const runner = this.getRunner();
    runner.prepareCase(testCase.id);

    <view
      width="100%"
      height="100%"
      backgroundColor="#F8FAFC"
      paddingTop={insetTop}
      paddingRight={insetRight}
      paddingBottom={insetBottom}
      paddingLeft={insetLeft}
    >
      <view
        key={`snapshot-${testCase.id}`}
        ref={this.snapshotRef}
        width={SNAPSHOT_WIDTH}
        height={SNAPSHOT_HEIGHT}
        backgroundColor="#F8FAFC"
      >
        {testCase.render({
          caseId: testCase.id,
          rootRef: this.rootRef,
          targetRef: this.targetRef,
          record: (message: string) => runner.record(message),
        })}
      </view>
      <label
        value={`case ${currentIndex + 1}/${INTEGRATION_TEST_CASES.length}: ${testCase.id}${this.hasStarted ? '' : ' (warming up)'}`}
        position="absolute"
        left={10 + insetLeft}
        bottom={10 + insetBottom}
        font="system 11"
        color="#475569"
      />
    </view>;

    // Schedule after emitting the test body so the layout callback observes this render.
    this.scheduleCaptureForRenderedCase(testCase.id);
  }
}
