import { DialogButton, Field, Millennium, pluginSelf } from '@steambrew/client';
import { useEffect, useState } from 'react';
import { DispatchSystemColors } from '../patcher/SystemColors';
import { settingsClasses } from '../classes';
import { locale } from '../locales';

const RenderAccentColorPicker = ({ currentThemeUsesAccentColor }: { currentThemeUsesAccentColor: boolean }) => {
	const [colorState, setColorState] = useState(pluginSelf.systemColors.accent.substring(0, 7));

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

		Millennium.callServerMethod('cfg.change_accent_color', { new_color: hexColor }).then((result: any) => {
			DispatchSystemColors(JSON.parse(result));
			UpdateAllWindows();
		});
	};

	const ResetColor = () => {
		Millennium.callServerMethod('cfg.reset_accent_color').then((result: any) => {
			DispatchSystemColors(JSON.parse(result));
			setColorState(pluginSelf.systemColors.accent.substring(0, 7));
			UpdateAllWindows();
		});
	};

	return (
		<>
			<style>
				{`.DialogBody { margin-bottom: 48px; }
            input.colorPicker { margin-left: 10px !important; border: unset !important; min-width: 38px; width: 38px !important; height: 38px; !important; background: transparent; padding: unset !important; }`}
			</style>

			<Field
				label={locale.themePanelCustomAccentColor}
				description={
					<>
						<div>{locale.themePanelCustomAccentColorToolTip}</div>
						{currentThemeUsesAccentColor !== undefined ? (
							<div className={currentThemeUsesAccentColor ? 'themeDoesUseAccent' : 'themeDoesNotUseAccent'}>
								{locale.themePanelCustomColorNotUsed}
							</div>
						) : (
							<div>Checking theme information...</div>
						)}
					</>
				}
			>
				{
					<DialogButton className={settingsClasses.SettingsDialogButton} onClick={ResetColor}>
						Reset
					</DialogButton>
				}
				<input
					type="color"
					className="colorPicker"
					name="colorPicker"
					value={colorState}
					onChange={(event) => UpdateColor(event.target.value)}
				/>
			</Field>
		</>
	);
};

export { RenderAccentColorPicker };
