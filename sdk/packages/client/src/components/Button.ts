import { FC } from 'react';

import { DialogButton, DialogButtonProps } from './Dialog';

export interface ButtonProps extends DialogButtonProps {}

/** @component React Components */
export const Button = (DialogButton as any)?.render({}).type as FC<ButtonProps>;
