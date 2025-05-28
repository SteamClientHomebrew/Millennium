import { DialogButton, Field, pluginSelf } from '@steambrew/client';
import { settingsClasses } from '../utils/classes';
import { locale } from '../../locales';
import { settingsManager } from '../settings-manager';
import { useState, useRef } from 'react';
import { PyChangeAccentColor } from '../utils/ffi';

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
	const debounceTimer = useRef<NodeJS.Timeout | null>(null);
	const [accentColor, setAccentColor] = useState<string>(pluginSelf.accentColor.accent);
	const isDefaultColor = SanitizeHex(pluginSelf.accentColor.accent) === SanitizeHex(pluginSelf.accentColor.originalAccent);

	const UpdateAllWindows = () => {
		g_PopupManager.m_mapPopups.data_.forEach((element: any) => {
			element.value_.m_popup.window.document.querySelectorAll('#SystemAccentColorInject').forEach((element: any) => {
				element.innerText = settingsManager.config.general.accentColor;
			});
		});
	};

	const ResetColor = () => handleColorChange(pluginSelf.accentColor.originalAccent);

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

	function handleColorChange(newColor: string) {
		const sanitizedColor = SanitizeHex(newColor);

		setAccentColor(sanitizedColor);
		pluginSelf.accentColor.accent = sanitizedColor;

		if (debounceTimer.current) {
			clearTimeout(debounceTimer.current);
		}

		debounceTimer.current = setTimeout(() => {
			setAccentColor(sanitizedColor);
			UpdateAllWindows();
			PyChangeAccentColor({ new_color: sanitizedColor });
		}, 300);
	}

	return (
		<Field
			className="MillenniumThemes_AccentColorField"
			data-default-color={isDefaultColor}
			label={locale.themePanelCustomAccentColor}
			description={locale.themePanelCustomAccentColorToolTip}
			bottomSeparator="none"
		>
			<ResetButton />
			<input type="color" className="MillenniumColorPicker" value={SanitizeHex(accentColor)} onChange={(event) => handleColorChange(event.target.value)} />
		</Field>
	);
};

export { RenderAccentColorPicker };
