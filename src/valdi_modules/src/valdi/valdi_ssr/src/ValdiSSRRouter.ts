import type { ComponentConstructor, IComponent } from 'valdi_core/src/IComponent';
import {
  HTTPServer,
  type HTTPServerRequest,
  type HTTPServerResponse,
  type HTTPServerResponseStream,
} from 'valdi_http/src/HTTPServer';
import { ValdiHTMLRenderer } from './ValdiHTMLRenderer';

export type ValdiSSRRequest = HTTPServerRequest;

export type ValdiSSRViewModelRenderer<ViewModel> = (viewModel: ViewModel) => Promise<void>;
export type ValdiSSRViewModelStream<ViewModel> = (render: ValdiSSRViewModelRenderer<ViewModel>) => () => void;

export interface ValdiSSRRenderInput<ViewModel, ComponentContext> {
  readonly viewModel: ViewModel;
  readonly componentContext: ComponentContext;
  /** Starts producing later view models once the streaming response is open. */
  readonly startViewModelStream?: ValdiSSRViewModelStream<ViewModel>;
}

export type ValdiSSRRouteBuilder<ViewModel, ComponentContext> = (
  request: ValdiSSRRequest,
) => ValdiSSRRenderInput<ViewModel, ComponentContext> | Promise<ValdiSSRRenderInput<ViewModel, ComponentContext>>;

export interface ValdiSSRServerAddress {
  readonly port: number;
  readonly url: string;
}

interface ValdiSSRRoute {
  readonly componentClass: ComponentConstructor<IComponent>;
  readonly build: ValdiSSRRouteBuilder<unknown, unknown>;
}

export class ValdiSSRRequestError extends Error {
  readonly statusCode: number;

  constructor(statusCode: number, message: string) {
    super(message);
    this.statusCode = statusCode;
  }
}

export class ValdiSSRRouter {
  private readonly routes: { [path: string]: ValdiSSRRoute | undefined } = {};
  private readonly server: HTTPServer;
  private listening = false;

  constructor(private readonly documentTitle: string) {
    this.server = new HTTPServer(request => this.handleRequest(request));
  }

  add<ViewModel, ComponentContext>(
    routePath: string,
    componentClass: ComponentConstructor<IComponent<ViewModel, ComponentContext>, ViewModel, ComponentContext>,
    build: ValdiSSRRouteBuilder<ViewModel, ComponentContext>,
  ): void {
    const normalizedPath = normalizeRoutePath(routePath);
    if (this.routes[normalizedPath] !== undefined) {
      throw new Error(`A Valdi SSR route is already registered for ${normalizedPath}`);
    }
    this.routes[normalizedPath] = {
      componentClass: componentClass as ComponentConstructor<IComponent>,
      build: build as ValdiSSRRouteBuilder<unknown, unknown>,
    };
  }

  async listen(port: number): Promise<ValdiSSRServerAddress> {
    if (this.listening) {
      throw new Error('ValdiSSRRouter is already listening');
    }
    await this.server.start(port);
    this.listening = true;
    return {
      port: this.server.port,
      url: `http://127.0.0.1:${this.server.port}`,
    };
  }

  close(): void {
    this.server.stop();
    this.listening = false;
  }

  private async handleRequest(request: HTTPServerRequest): Promise<HTTPServerResponse> {
    try {
      if (request.method === 'GET' && request.path === '/health') {
        return textResponse(200, 'ok\n');
      }
      if (request.method !== 'GET') {
        throw new ValdiSSRRequestError(405, 'Only GET requests are supported');
      }
      const route = this.routes[normalizeRoutePath(request.path)];
      if (route === undefined) {
        const routeList = Object.keys(this.routes).sort().join(', ');
        throw new ValdiSSRRequestError(404, `No Valdi SSR route for ${request.path}. Registered routes: ${routeList}`);
      }
      return await this.streamRoute(request, route);
    } catch (error) {
      const statusCode = error instanceof ValdiSSRRequestError ? error.statusCode : 500;
      const message = error instanceof Error ? error.message : String(error);
      return textResponse(statusCode, `${message}\n`);
    }
  }

  private async streamRoute(request: HTTPServerRequest, route: ValdiSSRRoute): Promise<HTTPServerResponse> {
    const input = await route.build(request);
    let responseStream: HTTPServerResponseStream | undefined;
    const renderer = new ValdiHTMLRenderer(route.componentClass, input.componentContext, {
      hasUpdatedHtml: () => {
        if (responseStream !== undefined) {
          responseStream.write(renderer.currentHtml());
        }
      },
    });
    try {
      await renderer.render(input.viewModel);
    } catch (error) {
      renderer.destroy();
      throw error;
    }

    const response: HTTPServerResponse = {
      statusCode: 200,
      headers: {
        'cache-control': 'no-store',
        'content-type': 'text/html; charset=utf-8',
        'x-content-type-options': 'nosniff',
      },
      body:
        `<!doctype html><html><head><meta charset="utf-8"><title>${escapeHtml(this.documentTitle)}</title>` +
        '</head><body>' +
        renderer.currentHtml(),
    };
    const startViewModelStream = input.startViewModelStream;
    if (startViewModelStream === undefined) {
      renderer.destroy();
      return response;
    }

    return {
      ...response,
      stream: stream => {
        responseStream = stream;
        let stopViewModelStream: (() => void) | undefined;
        stream.onClose(() => {
          stopViewModelStream?.();
          renderer.destroy();
        });
        stopViewModelStream = startViewModelStream(async viewModel => {
          if (stream.closed) {
            return;
          }
          try {
            await renderer.render(viewModel);
          } catch (error) {
            console.error('Valdi SSR view-model stream failed', error);
            stream.close();
          }
        });
      },
    };
  }
}

function normalizeRoutePath(routePath: string): string {
  if (!routePath.startsWith('/')) {
    throw new Error(`Valdi SSR route paths must start with '/': ${routePath}`);
  }
  if (routePath.length > 1 && routePath.endsWith('/')) {
    return routePath.slice(0, -1);
  }
  return routePath;
}

function textResponse(statusCode: number, text: string): HTTPServerResponse {
  return {
    statusCode,
    headers: {
      'cache-control': 'no-store',
      'content-type': 'text/plain; charset=utf-8',
      'x-content-type-options': 'nosniff',
    },
    body: text,
  };
}

function escapeHtml(value: string): string {
  return value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
