import { FC } from 'react';

import { CommonUIModule, sourceOf } from '../module-registry';

export interface ToggleProps {
	value: boolean;
	disabled?: boolean;
	onChange?(checked: boolean): void;
	navRef?: any; // TODO figure out what this is
}

/** @component React Components */
export const Toggle = Object.values(CommonUIModule).find((mod: any) => sourceOf(mod?.render).includes('.ToggleOff)')) as FC<ToggleProps>;
