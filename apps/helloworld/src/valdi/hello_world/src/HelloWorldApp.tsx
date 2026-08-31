import { Component } from 'valdi_core/src/Component';
import { Style } from 'valdi_core/src/Style';
import { systemFont } from 'valdi_core/src/SystemFont';
import { Label, ScrollView } from 'valdi_tsx/src/NativeTemplateElements';

import res from '../res';
import { ByteExactImageExample } from './ByteExactImageExample';
import { onRootComponentCreated } from './CppModule';
import {
  APP_NAME,
  reproduceTeardownCrash,
  reproduceTeardownDegraded,
  reproduceTeardownInvocationCrash,
} from './NativeModule';

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

/**
 * @Component
 * @ExportModel
 */
export class App extends Component<ViewModel, ComponentContext> {
  onCreate(): void {
    onRootComponentCreated(this.renderer.contextId);
    console.log('Hello World onCreate!');
  }

  onRender(): void {
    console.log('Hello World onRender!!!');
    <view backgroundColor="white">
      {/* accessibilityId is how instrumented tests target an element — see
          apps/helloworld/androidTest. */}
      <scroll style={styles.scroll} padding={16} accessibilityId="hello-world-scroll">
        <layout marginTop={100} flexDirection="row" width="100%" minHeight={10}>
          <image src={res.emoji} height="100%" tint="#808080" marginRight={10} />
          <label
            style={styles.title}
            value={`Welcome to ${APP_NAME}!`}
            font={systemFont(20)}
            accessibilityId="welcome-label"
          />
        </layout>
        <ByteExactImageExample />
        {/* Debug-only: resolve a bridge function after teardown with the resolution-teardown
            degrade ON (shipped default) — returns a no-op, no crash. iOS-only; no-op on
            android/web. */}
        <view
          marginTop={24}
          padding={12}
          backgroundColor="#eef7ee"
          borderRadius={8}
          onTap={reproduceTeardownDegraded}
          accessibilityId="teardown-degrade-button"
        >
          <label
            value="✅ Resolve after teardown (degrade ON — no crash)"
            font={systemFont(16)}
            style={styles.reproSafeLabel}
          />
        </view>
        {/* Debug-only: same resolution with the degrade kill switch OFF — raises an uncatchable
            SCValdiError and aborts (SIGABRT), reproducing the original teardown crash. */}
        <view
          marginTop={12}
          padding={12}
          backgroundColor="#f7eeee"
          borderRadius={8}
          onTap={reproduceTeardownCrash}
          accessibilityId="teardown-crash-button"
        >
          <label
            value="⚠️ Resolve after teardown (degrade OFF — SIGABRT)"
            font={systemFont(16)}
            style={styles.reproCrashLabel}
          />
        </view>
        {/* Debug-only: resolve (degrades to a no-op) then INVOKE after teardown — the invocation
            returns a null value in a nonnull slot; passing it to a nonnull-requiring API
            (+[NSURL fileURLWithPath:]) aborts (SIGABRT). Shows the degrade relocating a resolution
            abort into an invocation nil-in-nonnull crash. */}
        <view
          marginTop={12}
          padding={12}
          backgroundColor="#f7eeee"
          borderRadius={8}
          onTap={reproduceTeardownInvocationCrash}
          accessibilityId="teardown-invocation-button"
        >
          <label
            value="💥 Invoke after teardown (nil → nonnull → SIGABRT)"
            font={systemFont(16)}
            style={styles.reproCrashLabel}
          />
        </view>
      </scroll>
    </view>;
  }
}

const styles = {
  scroll: new Style<ScrollView>({
    alignItems: 'center',
    height: '100%',
  }),

  title: new Style<Label>({
    color: 'black',
    accessibilityCategory: 'header',
    width: '100%',
  }),

  reproSafeLabel: new Style<Label>({
    color: '#1b5e20',
  }),

  reproCrashLabel: new Style<Label>({
    color: '#b00020',
  }),
};
