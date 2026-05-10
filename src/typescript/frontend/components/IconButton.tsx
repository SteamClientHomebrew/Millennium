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
	tooltip?: string;
}


interface IconButtonWithTextProps extends IconButtonProps {
    text?: string;
}

export function IconButton(props: IconButtonProps) {
	const { disabled, name, onClick, tooltip } = props;
	const className = joinClassNames('MillenniumButton', 'MillenniumIconButton', settingsClasses.SettingsDialogButton, props.className ?? '');
	const iconComp = IconsModule[name as keyof typeof IconsModule] ?? 'span';
	const icon = createElement(iconComp as React.ElementType);

	const Button = () => (
		<DialogButton className={className} disabled={disabled} onClick={onClick} data-icon-name={name}>
			{icon}
		</DialogButton>
	);

	if (!tooltip) {
		return <Button />;
	}

	return (
		<DesktopTooltip toolTipContent={tooltip}>
			<Button />
		</DesktopTooltip>
	);
}

export function IconButtonWithText(props: IconButtonWithTextProps) {
	const { disabled, name, onClick, tooltip, text } = props;
	const className = joinClassNames('MillenniumButton', 'MillenniumIconButton', 'MillenniumIconButtonWithText', settingsClasses.SettingsDialogButton, props.className ?? '');
	const iconComp = IconsModule[name as keyof typeof IconsModule] ?? 'span';
	const icon = createElement(iconComp as React.ElementType);

	const Button = () => (
		<DialogButton className={className} disabled={disabled} onClick={onClick} data-icon-name={name}>
            {icon}
			{text}
		</DialogButton>
	);

	if (!tooltip) {
		return <Button />;
	}

	return (
		<DesktopTooltip toolTipContent={tooltip}>
			<Button />
		</DesktopTooltip>
	);
}
