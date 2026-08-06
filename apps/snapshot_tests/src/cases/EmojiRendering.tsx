import { Component } from 'valdi_core/src/Component';
import { testFont } from './TestFont';

/**
 * Pixel coverage for emoji glyph fallback.
 *
 * The primary snapshot font (Roboto Mono) has no emoji glyphs, so every emoji here can only render
 * via per-character font fallback into the registered emoji family. That makes this case a real
 * check on the fallback path rather than on the primary font: if fallback stops resolving, the
 * glyphs collapse to missing-glyph boxes and the diff is unmissable.
 *
 * It also covers the two things that had to be fixed before an emoji font could be registered from
 * TypeScript at all — registration by file path, and `canUseAsFallback`. See main.tsx.
 */
const styles = {
  root: {
    width: '100%' as const,
    height: '100%' as const,
    backgroundColor: '#FFFFFF',
    flexDirection: 'column' as const,
    padding: 8,
  },
  section: {
    marginBottom: 10,
  },
};

export class EmojiRendering extends Component {
  onRender(): void {
    <view {...styles.root}>
      <label value="emoji fallback tests" font={testFont(10)} color="#999999" />
      <layout height={8} />

      <layout {...styles.section}>
        <label value="Emoji only:" font={testFont(9)} color="#666666" />
        <label value="😀🎉🔥" font={testFont(24)} color="#000000" />
      </layout>

      <layout {...styles.section}>
        <label value="Mixed with Latin (fallback mid-run):" font={testFont(9)} color="#666666" />
        <label value="ok 👍 done" font={testFont(20)} color="#000000" />
      </layout>

      <layout {...styles.section}>
        <label value="Multi-codepoint (ZWJ sequence):" font={testFont(9)} color="#666666" />
        <label value="👩‍💻" font={testFont(24)} color="#000000" />
      </layout>

      <layout {...styles.section}>
        <label value="Skin-tone modifier:" font={testFont(9)} color="#666666" />
        <label value="👋🏽" font={testFont(24)} color="#000000" />
      </layout>

      <layout {...styles.section}>
        <label value="Wrapping across lines:" font={testFont(9)} color="#666666" />
        <label value="😀 a 🎉 b 🔥 c 👍 d 😀 e 🎉 f" font={testFont(16)} color="#000000" />
      </layout>
    </view>;
  }
}
