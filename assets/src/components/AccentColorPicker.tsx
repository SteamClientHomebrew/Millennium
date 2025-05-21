import { DialogButton, Field } from '@steambrew/client';
import { settingsClasses } from '../utils/classes';
import { locale } from '../../locales';
import { useMillenniumState, useUpdateConfig } from '../config-provider';
import { settingsManager } from '../settings-manager';

const SanitizeHex = (color: string) => {
	if (color.startsWith('#')) {
		if (color.length === 9 || color.length === 7) {
			color = color.substring(0, 7);
		}
		if (color.length === 4) {
			return `#${color[1]}${color[1]}${color[2]}${color[2]}${color[3]}${color[3]}`;
		}
	}
	return color;
};

const RenderAccentColorPicker = () => {
	const config = useMillenniumState();
	const updateConfig = useUpdateConfig();

	const isDefaultColor = SanitizeHex(config.general.accentColor.accent) === SanitizeHex(config.general.accentColor.originalAccent);

	const UpdateAllWindows = () => {
		g_PopupManager.m_mapPopups.data_.forEach((element: any) => {
			element.value_.m_popup.window.document.querySelectorAll('#SystemAccentColorInject').forEach((element: any) => {
				element.innerText = settingsManager.config.general.accentColor;
			});
		});
	};

	const UpdateColor = async (hexColor: string) => {
		updateConfig((draft) => {
			draft.general.accentColor.accent = hexColor;
		});
		UpdateAllWindows();
	};

	const ResetColor = () => UpdateColor(config.general.accentColor.originalAccent);

	const ResetButton = () => {
		if (isDefaultColor) {
			return <></>;
		}

		return (
			<DialogButton className={settingsClasses.SettingsDialogButton} onClick={ResetColor}>
				Reset
			</DialogButton>
		);
	};

	return (
		<Field
			className="MillenniumThemes_AccentColorField"
			data-default-color={isDefaultColor}
			label={locale.themePanelCustomAccentColor}
			description={locale.themePanelCustomAccentColorToolTip}
			bottomSeparator="none"
		>
			<ResetButton />
			<input type="color" className="MillenniumColorPicker" value={config.general.accentColor.accent} onChange={(event) => UpdateColor(event.target.value)} />
		</Field>
	);
};

export { RenderAccentColorPicker };
