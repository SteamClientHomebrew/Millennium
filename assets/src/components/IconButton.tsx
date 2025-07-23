import { DialogButton, type DialogButtonProps, joinClassNames } from '@steambrew/client';
import { settingsClasses } from '../utils/classes';

export function IconButton(props: DialogButtonProps) {
	const { children } = props;

	return (
		<DialogButton {...props} className={joinClassNames('MillenniumButton', 'MillenniumIconButton', settingsClasses.SettingsDialogButton, props.className)}>
			{children}
		</DialogButton>
	);
}
