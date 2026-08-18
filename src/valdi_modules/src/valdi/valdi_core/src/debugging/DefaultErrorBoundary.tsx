import { StatefulComponent } from '../Component';
import { getValdiRuntime } from '../ValdiRuntimeProvider';
import { withLazyPromise } from '../WithLazyPromise';
import { lazyPromise } from '../utils/LazyPromise';
import { RendererError } from '../utils/RendererError';
const ErrorComponent = withLazyPromise(lazyPromise(() => import('./ErrorComponent')).then(m => m.ErrorComponent));

interface State {
  error?: Error;
}

const runtime = getValdiRuntime();

/**
 * A component which catches errors and display an error screen
 * when an error is thrown.
 */
export class DefaultErrorBoundary extends StatefulComponent<{}, State> {
  state: State = {};

  onRender() {
    const error = this.state.error;
    if (error) {
      <ErrorComponent error={error} />;
    } else {
      <slot />;
    }
  }

  onError(error: Error) {
    if (error instanceof RendererError) {
      runtime.onUncaughtError(error.message, error.sourceError);
    } else {
      runtime.onUncaughtError(error.message, error);
    }

    if (!this.state.error) {
      this.setState({
        error,
      });
    }
  }
}
