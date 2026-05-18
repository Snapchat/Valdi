import { StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { TextField, View, WebViewElement } from 'valdi_tsx/src/NativeTemplateElements';
import { IWebViewController, IWebViewListener, WebView as WebViewModule } from 'valdi_webview/src/WebView';

import { IconButton } from './IconButton';
import res from './res';

const SNAPCHAT_URL = 'https://www.snapchat.com';
const BAR_HEIGHT = 56;

interface State {
  urlText: string;
}

export class App extends StatefulComponent<{}, State> {
  state: State = {
    urlText: SNAPCHAT_URL,
  };

  private readonly webViewController: IWebViewController = WebViewModule.createController();

  private readonly webViewListener: IWebViewListener = {
    onMessage(message: string): void {
      console.log(`WebView message: ${message}`);
    },
    onLoadFailed(errorMessage: string): void {
      console.log(`WebView load failed: ${errorMessage}`);
    },
    onLoadCompleted(): void {
      console.log('WebView load completed');
    },
  };

  onCreate(): void {
    this.registerDisposable(() => {
      this.webViewController.dispose();
    });

    this.webViewController.setListener(this.webViewListener);
    this.webViewController.load({ url: SNAPCHAT_URL });
  }

  onDestroy(): void {
    this.webViewController.setListener(undefined);
  }

  onRender(): void {
    <view style={styles.root}>
      <view
        style={styles.navigationBar}
        height={BAR_HEIGHT + Device.getDisplayTopInset()}
        paddingTop={Device.getDisplayTopInset()}
      >
        <IconButton icon={res.back} onTap={this.goBack} />
        <IconButton icon={res.forward} onTap={this.goForward} />
        <IconButton icon={res.reload} onTap={this.reload} />
        <view style={styles.urlFieldContainer}>
          <textfield
            style={styles.urlField}
            value={this.state.urlText}
            onChange={this.onUrlTextChange}
            onEditEnd={this.onUrlEditEnd}
          />
        </view>
      </view>
      <webview controller={this.webViewController} style={styles.webview} />
    </view>;
  }

  private goBack = (): void => {
    this.webViewController.getState().then(state => {
      if (state.canGoBack) {
        this.webViewController.goBack();
      }
    });
  };

  private goForward = (): void => {
    this.webViewController.getState().then(state => {
      if (state.canGoForward) {
        this.webViewController.goForward();
      }
    });
  };

  private reload = (): void => {
    this.webViewController.reload();
  };

  private readonly onUrlTextChange: NonNullable<TextField['onChange']> = event => {
    this.setState({ urlText: event.text });
  };

  private readonly onUrlEditEnd: NonNullable<TextField['onEditEnd']> = event => {
    this.loadUrl(event.text);
  };

  private loadUrl(rawUrl: string): void {
    const url = this.normalizeUrl(rawUrl);
    this.setState({ urlText: url });
    this.webViewController.load({ url });
  }

  private normalizeUrl(rawUrl: string): string {
    const trimmedUrl = rawUrl.trim();
    if (trimmedUrl.length === 0) {
      return SNAPCHAT_URL;
    }
    if (trimmedUrl.indexOf('://') >= 0) {
      return trimmedUrl;
    }
    return `https://${trimmedUrl}`;
  }
}

const styles = {
  root: new Style<View>({
    backgroundColor: 'white',
    width: '100%',
    height: '100%',
  }),

  navigationBar: new Style<View>({
    width: '100%',
    paddingLeft: 10,
    paddingRight: 10,
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#f8fafc',
  }),

  urlFieldContainer: new Style<View>({
    flexGrow: 1,
    height: 40,
    borderRadius: 10,
    backgroundColor: 'white',
    borderWidth: 1,
    borderColor: '#cbd5e1',
    paddingLeft: 12,
    paddingRight: 12,
    justifyContent: 'center',
  }),

  urlField: new Style<TextField>({
    width: '100%',
    height: 38,
    color: '#0f172a',
    tintColor: '#2563eb',
    font: systemFont(15),
    placeholder: 'Enter a URL',
    contentType: 'url',
    returnKeyText: 'go',
    autocapitalization: 'none',
    autocorrection: 'none',
    closesWhenReturnKeyPressed: true,
  }),

  webview: new Style<WebViewElement>({
    width: '100%',
    flexGrow: 1,
  }),
};
