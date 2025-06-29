/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { DialogButton, Field, pluginSelf } from '@steambrew/client';
import { settingsClasses } from '../utils/classes';
import { locale } from '../../locales';
import { useState, useRef } from 'react';
import { PyChangeAccentColor } from '../utils/ffi';
import { DispatchSystemColors } from '../patcher/SystemColors';
import { SystemAccentColor } from '../types';

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

	const UpdateAllWindows = (newColors: SystemAccentColor) => {
		g_PopupManager.m_mapPopups.data_.forEach((element: any) => {
			element.value_.m_popup.window.document.querySelectorAll('#SystemAccentColorInject').forEach((element: any) => {
				DispatchSystemColors(newColors);
				element.innerText = pluginSelf.systemColor;
			});
		});
	};

	const ResetColor = () => handleColorChange(pluginSelf.accentColor.originalAccent);

	function handleColorChange(newColor: string) {
		const sanitizedColor = SanitizeHex(newColor);

		setAccentColor(sanitizedColor);
		pluginSelf.accentColor.accent = sanitizedColor;

		if (debounceTimer.current) {
			clearTimeout(debounceTimer.current);
		}

		debounceTimer.current = setTimeout(async () => {
			setAccentColor(sanitizedColor);
			UpdateAllWindows(JSON.parse(await PyChangeAccentColor({ new_color: sanitizedColor })));
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
			<DialogButton className={settingsClasses.SettingsDialogButton} disabled={isDefaultColor} onClick={ResetColor}>
				Reset
			</DialogButton>
			<input type="color" className="MillenniumColorPicker" value={SanitizeHex(accentColor)} onChange={(event) => handleColorChange(event.target.value)} />
		</Field>
	);
};

export { RenderAccentColorPicker };
