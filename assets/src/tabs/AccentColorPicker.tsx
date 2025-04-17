import { callable, DialogButton, Field, Millennium, pluginSelf } from '@steambrew/client';
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

const ChangeAccentColor = callable<[{ new_color: string }], string>('cfg.change_accent_color');
const ResetAccentColor = callable<[], string>('cfg.reset_accent_color');

const RenderAccentColorPicker = ({ currentThemeUsesAccentColor }: { currentThemeUsesAccentColor: boolean }) => {
	const [colorState, setColorState] = useState(pluginSelf.systemColors.accent.substring(0, 7));
	const isDefaultColor = SanitizeHex(colorState) === SanitizeHex(pluginSelf.systemColors.originalAccent);

	const UpdateAllWindows = () => {
		// @ts-ignore
		g_PopupManager.m_mapPopups.data_.forEach((element: any) => {
			element.value_.m_popup.window.document.querySelectorAll('#SystemAccentColorInject').forEach((element: any) => {
				element.innerText = pluginSelf.systemColor;
			});
		});
	};

	const UpdateColor = async (hexColor: string) => {
		setColorState(hexColor);
		const result = await ChangeAccentColor({ new_color: hexColor });

		DispatchSystemColors(JSON.parse(result));
		UpdateAllWindows();
	};

	const ResetColor = async () => {
		const result = await ResetAccentColor();

		DispatchSystemColors(JSON.parse(result));
		setColorState(pluginSelf.systemColors.accent.substring(0, 7));
		UpdateAllWindows();
	};

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
			{!isDefaultColor && (
				<DialogButton className={settingsClasses.SettingsDialogButton} onClick={ResetColor}>
					Reset
				</DialogButton>
			)}
			<input type="color" className="MillenniumColorPicker" value={colorState} onChange={(event) => UpdateColor(event.target.value)} />
		</Field>
	);
};

export { RenderAccentColorPicker };
