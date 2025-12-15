import { FC } from 'react';

import { CommonUIModule } from '../webpack';
import { ItemProps } from './Item';
import { createPropListRegex } from '../utils';

export interface ButtonItemProps extends ItemProps {
	onClick?(e: MouseEvent): void;
	disabled?: boolean;
}
const buttonItemRegex = createPropListRegex(['highlightOnFocus', 'childrenContainerWidth'], false);
/** @component React Components */
export const ButtonItem = Object.values(CommonUIModule).find(
	(mod: any) => (mod?.render?.toString && buttonItemRegex.test(mod.render.toString())) || mod?.render?.toString?.().includes('childrenContainerWidth:"min"'),
) as FC<ButtonItemProps>;
