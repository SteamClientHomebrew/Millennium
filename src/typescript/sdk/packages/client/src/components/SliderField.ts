import { FC } from 'react';

import { CommonUIModule } from '../webpack';
import { ItemProps } from './Item';

export interface NotchLabel {
	notchIndex: number;
	label: string;
	value?: number;
}

export interface SliderFieldProps extends ItemProps {
	value: number;
	min?: number;
	max?: number;
	step?: number;
	notchCount?: number;
	notchLabels?: NotchLabel[];
	notchTicksVisible?: boolean;
	showValue?: boolean;
	resetValue?: number;
	disabled?: boolean;
	editableValue?: boolean;
	validValues?: 'steps' | 'range' | ((value: number) => boolean);
	valueSuffix?: string;
	minimumDpadGranularity?: number;
	onChange?(value: number): void;
	className?: string;
}

/** @component React Components */
export const SliderField = Object.values(CommonUIModule).find(
	(mod: any) =>
		// stable || beta as of oct 2 2024
		mod?.toString?.()?.includes('SliderField,fallback') || mod?.toString?.()?.includes('SliderField",'),
) as FC<SliderFieldProps>;
