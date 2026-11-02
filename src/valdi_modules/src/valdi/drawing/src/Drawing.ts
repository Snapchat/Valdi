import { RequireFunc } from 'valdi_core/src/IModuleLoader';
import { DrawingModuleProvider } from './DrawingModuleProvider';

declare const require: RequireFunc;

// eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
export const Drawing = (require('Drawing') as DrawingModuleProvider).Drawing;
