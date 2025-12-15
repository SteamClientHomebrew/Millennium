import { FC } from 'react';

import { CommonUIModule } from '../webpack';
import { ItemProps } from './Item';

export interface ToggleFieldProps extends ItemProps {
	highlightOnFocus?: boolean;
	checked: boolean;
	disabled?: boolean;
	onChange?(checked: boolean): void;
}

/** @component React Components */
export const ToggleField = Object.values(CommonUIModule).find((mod: any) => mod?.render?.toString()?.includes('ToggleField,fallback')) as FC<ToggleFieldProps>;
