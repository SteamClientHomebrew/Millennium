import { FC } from 'react';

import { CommonUIModule, sourceOf } from '../module-registry';
import { ItemProps } from './Item';
import { createPropListRegex } from '../utils';

export interface ButtonItemProps extends ItemProps {
	onClick?(e: MouseEvent): void;
	disabled?: boolean;
}
const buttonItemRegex = createPropListRegex(['highlightOnFocus', 'childrenContainerWidth'], false);
/** @component React Components */
export const ButtonItem = Object.values(CommonUIModule).find(
	(mod: any) => buttonItemRegex.test(sourceOf(mod?.render)) || sourceOf(mod?.render).includes('childrenContainerWidth:"min"'),
) as FC<ButtonItemProps>;
