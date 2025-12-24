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

import { DialogButton, Dropdown, ErrorBoundary, PanelSection, PanelSectionRow } from '@steambrew/client';
import { ThemeItem } from '../types';
import { DesktopSideBarFocusedItemType, useDesktopMenu } from './DesktopMenuContext';
import { DesktopTooltip, SettingsDialogSubHeader } from '../components/SteamComponents';
import { RenderThemeEditor } from '../components/ThemeEditor';
import { locale } from '../utils/localization-manager';

enum ThemeDisabledReason {
	NONE,
	NOT_CONFIGURABLE,
}

const ThemeItem = ({ disabledReason, onClick, theme }: { disabledReason: ThemeDisabledReason; onClick: () => void; theme: ThemeItem }) => {
	const disabled = disabledReason !== ThemeDisabledReason.NONE;

	return (
		<DesktopTooltip toolTipContent={locale.themeIsNotConfigurable} direction="left" bDisabled={!disabled}>
			<PanelSectionRow key={theme?.native}>
				<DialogButton
					className="MillenniumButton MillenniumDesktopSidebar_LibraryItemButton"
					disabled={disabled}
					onClick={onClick}
					data-disabled-reason={disabled ? ThemeDisabledReason[disabledReason] : undefined}
				>
					{theme?.data?.name}
				</DialogButton>
			</PanelSectionRow>
		</DesktopTooltip>
	);
};

export const RenderThemeViews = ({ theme }: { theme: ThemeItem }) => {
	const { setFocusedItem } = useDesktopMenu();
	const isConfigurable = theme?.data?.Conditions || theme?.data?.RootColors;

	let disabledReason = ThemeDisabledReason.NONE;
	if (!isConfigurable) {
		disabledReason = ThemeDisabledReason.NOT_CONFIGURABLE;
	}

	return <ThemeItem disabledReason={disabledReason} onClick={setFocusedItem.spread(theme, DesktopSideBarFocusedItemType.THEME)} theme={theme} />;
};

export const RenderThemeView = () => {
	const { focusedItem } = useDesktopMenu();
	return (
		<ErrorBoundary>
			<RenderThemeEditor theme={focusedItem as ThemeItem} isSidebar={true} />
		</ErrorBoundary>
	);
};

export const ThemeSelectorView = () => {
	const { themes } = useDesktopMenu();

	if (!themes || !themes?.length) {
		return null; /** let parent component handle empty state */
	}

	return (
		<PanelSection>
			<SettingsDialogSubHeader>Theme Settings</SettingsDialogSubHeader>
			{(themes as ThemeItem[])?.map((theme) => (
				<RenderThemeViews key={theme.native} theme={theme} />
			))}
		</PanelSection>
	);
};
