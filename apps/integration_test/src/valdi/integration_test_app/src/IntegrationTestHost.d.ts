import { NativeNode } from 'valdi_tsx/src/NativeNode';

/**
 * @ExportModule
 */

// @ExportFunction
export function getPlatform(): string;

// @ExportFunction
export function getOutputPath(): string;

// @ExportFunction
export function markFinished(path: string): void;

// @ExportFunction
export function writeTextFile(path: string, contents: string): void;

// @ExportFunction
export function submitTouchSequence(node: NativeNode, sequenceJson: string): string;

// @ExportFunction
export function focusTextInput(node: NativeNode): string;

// @ExportFunction
export function replaceText(node: NativeNode, value: string): string;

// @ExportFunction
export function pressReturn(node: NativeNode): string;

// @ExportFunction
export function pressBackspace(node: NativeNode): string;
