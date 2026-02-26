import { ElementType, FC, ReactNode } from 'react';

import { Export, findModuleExport } from '../webpack';

export interface FocusRingProps {
	className?: string;
	rootClassName?: string;
	render?: ElementType;
	children?: ReactNode;
	NavigationManager?: any;
}

/** @component React Components */
export const FocusRing = findModuleExport((e: Export) => e?.toString()?.includes('.GetShowDebugFocusRing())')) as FC<FocusRingProps>;
