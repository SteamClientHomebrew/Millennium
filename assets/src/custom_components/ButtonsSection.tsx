import { DialogControlsSection } from '@steambrew/client';
import type { ReactNode, FC } from 'react';

interface ButtonsSectionProps {
	children: ReactNode;
}

export const ButtonsSection: FC<ButtonsSectionProps> = ({ children }) => (
	<DialogControlsSection className="MillenniumButtonsSection">{children}</DialogControlsSection>
);
