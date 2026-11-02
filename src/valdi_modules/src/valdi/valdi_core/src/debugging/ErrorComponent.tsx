import { Component } from '../Component';
import { IRenderedVirtualNode } from '../IRenderedVirtualNode';
import { getVirtualNodeDataFromRootToChild } from '../IRenderedVirtualNodeData';
import { symbolicate } from '../Symbolicator';
import { CompilerError } from '../utils/CompilerError';
import { RendererError } from '../utils/RendererError';
import { when } from '../utils/When';
import { colors, fonts, styles } from './Styles';
import { VirtualNodePathDisplayer } from './VirtualNodePathDisplayer';

interface ViewModel {
  error: Error;
}

interface ProcessedError {
  errorTitle: string;
  errorMessage: string;
  hasStacktrace: boolean;
  consoleMessage?: string;
  lastRenderedNode?: IRenderedVirtualNode;
}

/**
 * Component used when there is a compilation error
 */
export class ErrorComponent extends Component<ViewModel> {
  private processedError?: ProcessedError;

  onViewModelUpdate() {
    const error = this.viewModel.error;

    let errorTitle: string;
    let errorMessage: string;
    let hasStacktrace = false;
    let consoleMessage: string | undefined;
    let lastRenderedNode: IRenderedVirtualNode | undefined;

    if (error instanceof CompilerError) {
      errorTitle = 'Valdi Compiler Error';
      const errorMessageLines = error.message.split('\n');
      if (errorMessageLines.length > 1) {
        errorMessage = errorMessageLines[0];
        consoleMessage = error.message;
      } else {
        errorMessage = error.message;
      }
    } else if (error instanceof RendererError) {
      errorTitle = `Runtime Error: ${error.message}`;
      errorMessage = error.sourceError.message;
      lastRenderedNode = error.lastRenderedNode;

      const symbolicatedStack = symbolicate(error.sourceError);
      if (symbolicatedStack.stack) {
        consoleMessage = symbolicatedStack.stack;
        hasStacktrace = true;
      }
    } else {
      errorTitle = `Unknown Error`;
      errorMessage = error.message;

      const symbolicatedStack = symbolicate(error);
      if (symbolicatedStack.stack) {
        consoleMessage = symbolicatedStack.stack;
        hasStacktrace = true;
      }
    }

    this.processedError = { errorMessage, errorTitle, hasStacktrace, consoleMessage, lastRenderedNode };
  }

  onRender() {
    const processedError = this.processedError;
    if (!processedError) {
      return;
    }

    <view width="100%" height="100%" backgroundColor="white" alignItems="center" justifyContent="center" padding={20}>
      <scroll>
        <label
          textAlign="center"
          numberOfLines={0}
          font={fonts.title}
          color={colors.errorText}
          marginBottom={20}
          value={processedError.errorTitle}
        />
        <label
          textAlign="center"
          numberOfLines={0}
          style={styles.infoText}
          value={`'${processedError.errorMessage}'`}
        />
        {when(processedError.lastRenderedNode, lastRenderedNode => {
          const pair = getVirtualNodeDataFromRootToChild(lastRenderedNode);

          <layout height={20} />;
          <VirtualNodePathDisplayer node={pair.root} focusedNode={pair.child} />;
        })}
        {when(processedError.consoleMessage, consoleMessage => {
          <layout marginTop={20}>
            {when(processedError.hasStacktrace, () => {
              <label style={styles.infoText} value="Stacktrace:" marginBottom={8} />;
            })}
            <view style={styles.console}>
              <label numberOfLines={0} style={styles.consoleText} value={consoleMessage} />
            </view>
          </layout>;
        })}
      </scroll>
    </view>;
  }
}
