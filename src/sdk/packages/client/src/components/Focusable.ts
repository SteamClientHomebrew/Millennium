import { HTMLAttributes, ReactNode, RefAttributes, FC } from 'react';

import { Export, findModuleExport } from '../webpack';
import { FooterLegendProps } from './FooterLegend';
import { createPropListRegex } from '../utils';

export interface FocusableProps extends HTMLAttributes<HTMLDivElement>, FooterLegendProps {
	children: ReactNode;
	'flow-children'?: string;
	focusClassName?: string;
	focusWithinClassName?: string;
	noFocusRing?: boolean;
	onActivate?: (e: CustomEvent) => void;
	onCancel?: (e: CustomEvent) => void;
}

const focusableRegex = createPropListRegex(['flow-children', 'onActivate', 'onCancel', 'focusClassName', 'focusWithinClassName']);

/** @component React Components */
export const Focusable = findModuleExport(
	(e: Export) => (typeof e == 'function' && e?.toString && focusableRegex.test(e.toString())) || (e?.render?.toString && focusableRegex.test(e.render.toString())),
) as FC<FocusableProps & RefAttributes<HTMLDivElement>>;
