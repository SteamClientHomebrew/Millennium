import { DialogButton, IconsModule, joinClassNames } from '@steambrew/client';
import React, { createElement, MouseEvent } from 'react';
import { DesktopTooltip } from './SteamComponents';
import { settingsClasses } from '../utils/classes';

interface IconButtonProps {
	/**
	 * Button's class name.
	 * @default "MillenniumButton MillenniumIconButton MenuBarEditor_IconButton"
	 */
	className?: string;

	/**
	 * Is the button disabled? Tooltip will be shown regardless of its value.
	 */
	disabled?: boolean;

	/**
	 * Icon name from {@link IconsModule}.
	 */
	name: string;

	onClick: (ev?: MouseEvent) => void;

	/**
	 * Localization token for the tooltip.
	 */
	text?: string;
}

export function IconButton(props: IconButtonProps) {
	const { disabled, name, onClick, text } = props;
	const className = joinClassNames('MillenniumButton', 'MillenniumIconButton', settingsClasses.SettingsDialogButton, props.className ?? '');
	const iconComp = IconsModule[name as keyof typeof IconsModule] ?? 'span';
	const icon = createElement(iconComp as React.ElementType);

	const Button = () => (
		<DialogButton className={className} disabled={disabled} onClick={onClick} data-icon-name={name}>
			{icon}
		</DialogButton>
	);

	if (!text) {
		return <Button />;
	}

	return (
		<DesktopTooltip toolTipContent={text}>
			<Button />
		</DesktopTooltip>
	);
}
