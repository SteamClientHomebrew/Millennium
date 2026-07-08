import { HTMLAttributes, ReactNode, RefAttributes, FC } from 'react';

import { Export, findModuleExport } from '../webpack';

export interface CarouselProps extends HTMLAttributes<HTMLDivElement> {
	autoFocus?: boolean;
	enableBumperPaging?: boolean;
	fnDoesItemTakeFocus?: (...unknown: any[]) => boolean;
	fnGetColumnWidth?: (...unknown: any[]) => number;
	fnGetId?: (id: number) => number;
	fnItemRenderer?: (id: number, ...unknown: any[]) => ReactNode;
	fnUpdateArrows?: (...unknown: any[]) => any;
	initialColumn?: number;
	nHeight?: number;
	nIndexLeftmost?: number;
	nItemHeight?: number;
	nItemMarginX?: number;
	nNumItems?: number;
	name?: string;
	scrollToAlignment?: 'center';
}
/** @component React Components */
export const Carousel = findModuleExport((e: Export) => e.render?.toString().includes('setFocusedColumn:')) as FC<CarouselProps & RefAttributes<HTMLDivElement>>;
