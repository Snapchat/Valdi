import { AttributedText } from 'valdi_tsx/src/AttributedText';

/**
 * @NativeClass({
 *   marshallAsUntyped: true,
 *   ios: 'SCValdiWrappedValue', iosImportPrefix: 'valdi_core',
 *   android: 'com.snapchat.client.valdi.utils.CppObjectWrapper'
 * })
 */
export interface AttributedTextNative {
  __brand?: 'AttributedTextNative';
}

export function makeNativeAttributedText(attributedText: AttributedText): AttributedTextNative;
