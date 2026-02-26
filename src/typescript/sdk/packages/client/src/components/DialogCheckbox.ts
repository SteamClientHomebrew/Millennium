import { FC, ReactNode } from 'react';

import { findModuleExport } from '../webpack';
import { DialogCommonProps } from './Dialog';
import { FooterLegendProps } from './FooterLegend';

export interface DialogCheckboxProps extends Omit<DialogCommonProps, 'onChange'>, FooterLegendProps {
	onChange?(checked: boolean): void;
	label?: ReactNode;
	description?: ReactNode;
	disabled?: boolean;
	tooltip?: string;
	color?: string;
	highlightColor?: string;
	bottomSeparator?: 'standard' | 'thick' | 'none';
	controlled?: boolean;
	checked?: boolean;
}

/** @component React Components */
export const DialogCheckbox = findModuleExport(
	(e) =>
		e?.prototype &&
		typeof e?.prototype == 'object' &&
		'GetPanelElementProps' in e?.prototype &&
		'SetChecked' in e?.prototype &&
		'Toggle' in e?.prototype &&
		// beta || stable as of oct 2 2024
		(e?.prototype?.render?.toString?.().includes('="DialogCheckbox"') || (e.contextType && e.prototype?.render?.toString?.().includes('fallback:'))),
) as FC<DialogCheckboxProps>;
