import { Device } from 'valdi_core/src/Device';
import { setTimeoutInterruptible } from 'valdi_core/src/SetTimeout';
import { ElementFrame } from 'valdi_tsx/src/Geometry';
import { Label, Layout, View } from 'valdi_tsx/src/NativeTemplateElements';
import { StatefulComponent } from '../Component';
import { IRuntimeIssueObserver } from '../IRuntimeIssueObserver';
import { Style } from '../Style';
import { when } from '../utils/When';
import { DaemonClientManager, IDaemonClientManagerListener } from './DaemonClientManager';
import { DebugButton } from './DebugConsoleButton';
import { RuntimeIssue, RuntimeIssueDisplayer } from './RuntimeIssueDisplayer';

enum CollapseState {
  expanded,
  collapsed,
  expanding,
  collapsing,
}

interface State {
  reloaderAvailable: boolean; // we don't show the debug console if the reloader is not available
  collapseState: CollapseState;
  issues: RuntimeIssue[];
  issueIndex: number;
  topPadding: number;
  bottomPadding: number;
  rootHeight: number;
}

const viewNodePrefix = '[ViewNode:';

function parseRuntimeIssue(isError: boolean, message: string): RuntimeIssue {
  let resolvedMessage = message;
  let nodeId: number | undefined;

  if (message.startsWith(viewNodePrefix)) {
    const endIndex = message.indexOf(']');
    if (endIndex <= 0) {
      throw new Error('Failed to resolve closing bracket');
    }
    nodeId = Number.parseInt(message.substr(viewNodePrefix.length, message.length - endIndex));

    resolvedMessage = message.substr(endIndex + 2);
  }

  return {
    isError,
    message: resolvedMessage,
    nodeId,
  };
}

const headerHeight = 80;

const headerStyle = new Style<View>({
  backgroundColor: '#FFF300',
  width: '100%',
  boxShadow: '0 0 3 rgba(0, 0, 0, 0.15)',
});

const headerContainerStyle = new Style<Layout>({
  flexDirection: 'column',
  padding: 8,
  paddingBottom: 30,
  // height: headerHeight,
});

const topHeaderTopStyle = new Style<Layout>({
  flexDirection: 'row',
  justifyContent: 'space-between',
  flexGrow: 1,
});

const headerBottomStyle = new Style<Layout>({
  flexDirection: 'row',
  width: '100%',
  justifyContent: 'center',
});

const rootStyle = new Style<Layout>({
  width: '100%',
  height: '100%',
  position: 'absolute',
  flexDirection: 'column-reverse',
});

const issueTextSelectorStyle = new Style<Label>({
  font: 'Montserrat-Medium 13',
  margin: 8,
});

interface ViewModel {
  daemonClientManager: DaemonClientManager;
}

export class DebugConsole
  extends StatefulComponent<ViewModel, State>
  implements IDaemonClientManagerListener, IRuntimeIssueObserver
{
  state: State = {
    reloaderAvailable: false,
    collapseState: CollapseState.collapsed,
    issues: [],
    issueIndex: 0,
    bottomPadding: 0,
    topPadding: 0,
    rootHeight: 0,
  };
  private pendingNewIssues?: RuntimeIssue[];

  private displayHeight = Device.getDisplayHeight();

  onAvailabilityChanged(available: boolean) {
    this.setState({
      reloaderAvailable: available,
    });
  }

  onCreate() {
    const observer = Device.observeDisplayInsetChange(() => this.updateDisplayMargins());
    this.registerDisposable(observer.cancel);
    this.updateDisplayMargins();

    this.viewModel.daemonClientManager.addListener(this);
    this.renderer.addObserver(this);
  }

  onDestroy() {
    this.viewModel.daemonClientManager.removeListener(this);
    this.renderer.removeObserver(this);
  }

  onRender() {
    if (!this.state.reloaderAvailable) {
      return;
    }

    if (!this.state.issues.length) {
      return;
    }

    // Only render the debug component if it fits at least one third of the screen.
    const shouldRender = this.state.rootHeight > this.displayHeight / 3.0;

    const isExpanded = this.state.collapseState === CollapseState.expanded;

    <layout style={rootStyle} onLayout={this.onRootLayout}>
      <layout display={!shouldRender ? 'none' : undefined} flexGrow={isExpanded ? 1 : undefined} flexShrink={1}>
        <view
          style={headerStyle}
          paddingBottom={!isExpanded ? this.state.bottomPadding : undefined}
          paddingTop={isExpanded ? this.state.topPadding : undefined}
        >
          <layout style={headerContainerStyle}>
            <layout style={topHeaderTopStyle}>
              <DebugButton text='Dismiss' onTap={this.close} />
              <label value={`${this.state.issues.length} Valdi Warnings`} font='Montserrat-SemiBold 13' />
              <DebugButton
                text={this.state.collapseState === CollapseState.expanded ? 'Collapse' : 'Expand'}
                onTap={this.expandOrCollapse}
              />
            </layout>
            {this.renderHeaderBottom()}
          </layout>
        </view>

        {when(this.state.collapseState !== CollapseState.collapsed, () => this.renderBody())}
      </layout>
    </layout>;
  }

  private renderHeaderBottom() {
    <view
      style={headerBottomStyle}
      height={this.state.collapseState !== CollapseState.expanded ? 0 : undefined}
      opacity={this.state.collapseState === CollapseState.expanded ? 1.0 : 0}
    >
      <DebugButton onTap={this.goToPreviousIssue} text='<' />
      <label
        style={issueTextSelectorStyle}
        value={`Issue ${this.state.issueIndex + 1} / ${this.state.issues.length}`}
      />
      <DebugButton onTap={this.goToNextIssue} text='>' />
    </view>;
  }

  private updateDisplayMargins() {
    const topInset = Device.getDisplayTopInset();
    const bottomInset = Device.getDisplayBottomInset();

    this.setState({ bottomPadding: bottomInset, topPadding: topInset });
  }

  private goToPreviousIssue = () => {
    this.goToIssue(-1);
  };

  private goToNextIssue = () => {
    this.goToIssue(1);
  };

  private onRootLayout = (rootFrame: ElementFrame) => {
    this.setState({ rootHeight: rootFrame.height });
  };

  private goToIssue(increment: number) {
    const issuesLength = this.state.issues.length;
    if (!issuesLength) {
      return;
    }
    let nextIssue = this.state.issueIndex + increment;
    if (nextIssue < 0) {
      nextIssue = issuesLength - 1;
    } else if (nextIssue >= issuesLength) {
      nextIssue = 0;
    }
    this.setState({ issueIndex: nextIssue });
  }

  private renderBody() {
    const issue = this.state.issues[this.state.issueIndex];
    if (!issue) {
      return;
    }

    const isFullyExpanded = this.state.collapseState === CollapseState.expanded;

    <view
      backgroundColor='white'
      width='100%'
      flexGrow={1}
      flexShrink={1}
      padding={!isFullyExpanded ? 0 : 8}
      height={!isFullyExpanded ? 0 : undefined}
      marginBottom={isFullyExpanded ? this.state.bottomPadding : undefined}
    >
      <RuntimeIssueDisplayer issue={issue} />
    </view>;
  }

  private expandOrCollapse = () => {
    if (this.state.collapseState === CollapseState.collapsed) {
      this.setState({ collapseState: CollapseState.expanding });
      this.setStateAnimated({ collapseState: CollapseState.expanded }, { duration: 0.25 });
    } else if (this.state.collapseState === CollapseState.expanded) {
      // eslint-disable-next-line @typescript-eslint/no-floating-promises
      this.setStateAnimatedPromise({ collapseState: CollapseState.collapsing }, { duration: 0.25 }).then(() => {
        this.setState({ collapseState: CollapseState.collapsed });
      });
    }
  };

  private close = () => {
    this.setState({ issues: [], issueIndex: 0 });
  };

  private flushPendingIssues() {
    const pendingNewIssues = this.pendingNewIssues;
    if (!pendingNewIssues) {
      return;
    }
    this.pendingNewIssues = undefined;
    this.setState({ issues: [...this.state.issues, ...pendingNewIssues] });
  }

  private appendPendingIssue(issue: RuntimeIssue) {
    if (this.pendingNewIssues) {
      this.pendingNewIssues.push(issue);
      return;
    }

    this.pendingNewIssues = [issue];
    setTimeoutInterruptible(() => {
      this.flushPendingIssues();
    }, 0);
  }

  onRuntimeIssue(isError: boolean, message: string) {
    try {
      const issue = parseRuntimeIssue(isError, message);
      this.appendPendingIssue(issue);
    } catch (err: any) {
      console.error('Failed to parse runtime issue:', err);
    }
  }
}
