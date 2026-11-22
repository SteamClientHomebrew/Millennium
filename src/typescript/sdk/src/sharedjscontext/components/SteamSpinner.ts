import { FC, ReactNode, SVGAttributes } from 'react';

import { Export, findModuleExport, sourceOf } from '../module-registry';
import { nativeInternal } from '../internals';

interface SteamSpinnerProps {
	children?: ReactNode;
	background?: 'transparent'; // defaults to black seemingly, but only "transparent" is checked against
}

/** @component React Components */
export const SteamSpinner = (nativeInternal('SteamSpinner') ??
	findModuleExport((e: Export) => sourceOf(e).includes('Steam Spinner') && sourceOf(e).includes('src'))) as FC<SVGAttributes<SVGElement> & SteamSpinnerProps>;
