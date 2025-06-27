import React, { ReactNode } from 'react';
import { findModuleDetailsByExport, findModuleExport, ModalPosition } from '@steambrew/client';
import { FC } from 'react';
import { fieldClasses } from '../utils/classes';

interface BBCodeParserProps {
	bShowShortSpeakerInfo?: boolean;
	event?: any;
	languageOverride?: any;
	showErrorInfo?: boolean;
	text?: string;
}

export const BBCodeParser: FC<BBCodeParserProps> = findModuleExport((m) => typeof m === 'function' && m.toString().includes('this.ElementAccumulator'));

interface SettingsDialogSubHeaderProps extends React.HTMLProps<HTMLDivElement> {
	children: ReactNode;
}

export const SettingsDialogSubHeader: React.FC<SettingsDialogSubHeaderProps> = ({ children }) => <div className="SettingsDialogSubHeader">{children}</div>;

export const Separator: React.FC = () => <div className={fieldClasses.StandaloneFieldSeparator} />;

export const DesktopTooltip = findModuleDetailsByExport(
	(m) => m.toString().includes(`divProps`) && m.toString().includes(`tooltipProps`) && m.toString().includes(`toolTipContent`) && m.toString().includes(`tool-tip-source`),
)[1];

interface GenericConfirmDialogProps {
	children: ReactNode;
}

export const GenericConfirmDialog = ({ children }: GenericConfirmDialogProps) => (
	<ModalPosition>
		<div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
			<div className="DialogContent_InnerWidth">{children}</div>
		</div>
	</ModalPosition>
);
