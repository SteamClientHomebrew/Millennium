import { DialogButton, Field, Millennium, pluginSelf } from '@steambrew/client';
import { useEffect, useState } from 'react';
import { DispatchSystemColors } from '../patcher/SystemColors';
import { settingsClasses } from '../classes';
import { locale } from '../locales';

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

const RenderAccentColorPicker = ({ currentThemeUsesAccentColor }: { currentThemeUsesAccentColor: boolean }) => {
	const [colorState, setColorState] = useState(pluginSelf.systemColors.accent.substring(0, 7));
	const [isDefaultColor, setDefaultColor] = useState<boolean>(false);

	const UpdateAllWindows = () => {
		// @ts-ignore
		g_PopupManager.m_mapPopups.data_.forEach((element: any) => {
			element.value_.m_popup.window.document.querySelectorAll('#SystemAccentColorInject').forEach((element: any) => {
				element.innerText = pluginSelf.systemColor;
			});
		});
	};

	const UpdateColor = (hexColor: string) => {
		setColorState(hexColor);
		setDefaultColor(false);

		Millennium.callServerMethod('cfg.change_accent_color', { new_color: hexColor }).then((result: any) => {
			DispatchSystemColors(JSON.parse(result));
			UpdateAllWindows();
		});
	};

	const ResetColor = () => {
		Millennium.callServerMethod('cfg.reset_accent_color').then((result: any) => {
			DispatchSystemColors(JSON.parse(result));
			setColorState(pluginSelf.systemColors.accent.substring(0, 7));
			setDefaultColor(true);
			UpdateAllWindows();
		});
	};

	console.log('Current accent color:', colorState);
	console.log('Original accent color:', pluginSelf.systemColors.originalAccent);

	return (
		<Field
			className="MillenniumThemes_AccentColorField"
			data-default-color={isDefaultColor}
			label={locale.themePanelCustomAccentColor}
			description={
				<>
					<div>{locale.themePanelCustomAccentColorToolTip}</div>
					{currentThemeUsesAccentColor !== undefined ? (
						<div className="MillenniumThemes_AccentColorUsage" data-accent-color-in-use={currentThemeUsesAccentColor}>
							{locale.themePanelCustomColorNotUsed}
						</div>
					) : (
						<div>Checking theme information...</div>
					)}
				</>
			}
			bottomSeparator="none"
		>
			{SanitizeHex(colorState) !== SanitizeHex(pluginSelf.systemColors.originalAccent) && (
				<DialogButton className={settingsClasses.SettingsDialogButton} onClick={ResetColor}>
					Reset
				</DialogButton>
			)}
			<input type="color" className="MillenniumColorPicker" value={colorState} onChange={(event) => UpdateColor(event.target.value)} />
		</Field>
	);
};

export { RenderAccentColorPicker };
