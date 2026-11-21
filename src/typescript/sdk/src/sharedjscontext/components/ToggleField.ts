import { FC } from 'react';

import { CommonUIModule, sourceOf } from '../module-registry';
import { ItemProps } from './Item';

export interface ToggleFieldProps extends ItemProps {
	highlightOnFocus?: boolean;
	checked: boolean;
	disabled?: boolean;
	onChange?(checked: boolean): void;
}

/** @component React Components */
export const ToggleField = Object.values(CommonUIModule).find(
	(mod: any) =>
		// stable || beta as of oct 2 2024
		sourceOf(mod?.render).includes('ToggleField,fallback') || sourceOf(mod?.render).includes('ToggleField",'),
) as FC<ToggleFieldProps>;
