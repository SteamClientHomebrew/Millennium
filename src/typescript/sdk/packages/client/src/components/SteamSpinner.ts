import { FC, ReactNode, SVGAttributes } from 'react';

import { Export, findModuleExport } from '../webpack';

interface SteamSpinnerProps {
	children?: ReactNode;
	background?: 'transparent'; // defaults to black seemingly, but only "transparent" is checked against
}

/** @component React Components */
export const SteamSpinner = findModuleExport((e: Export) => e?.toString?.()?.includes('Steam Spinner') && e?.toString?.()?.includes('src')) as FC<
	SVGAttributes<SVGElement> & SteamSpinnerProps
>;
