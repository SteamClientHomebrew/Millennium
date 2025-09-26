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

import { fieldClasses, pagedSettingsClasses } from './classes';

const styles = `
:root {
	/* Stolen from Steam store */
	--MillenniumText-Font: "Motiva Sans", Arial, Sans-serif;
	--MillenniumText-HeadingLarge: normal 700 26px/1.4 var(--MillenniumText-Font);
	--MillenniumText-HeadingMedium: normal 700 22px/1.4 var(--MillenniumText-Font);
	--MillenniumText-HeadingSmall: normal 700 18px/1.4 var(--MillenniumText-Font);
	--MillenniumText-BodyLarge: normal 400 16px/1.4 var(--MillenniumText-Font);
	--MillenniumText-BodyMedium: normal 400 14px/1.4 var(--MillenniumText-Font);
	--MillenniumText-BodySmall: normal 400 12px/1.4 var(--MillenniumText-Font);

	--MillenniumTextColor-Normal: #fff;
	/* Matches body and .ModalPosition */
	--MillenniumTextColor-Muted: #969696;
	/* Base for these: #59bf40, stolen from Steam Guard icon in Settings -> Security */
	--MillenniumTextColor-Success: #59bf40;
	--MillenniumTextColor-Error: #bf4040;
	--MillenniumTextColor-Warning: #bfbd40;

	--MillenniumSpacing-Small: 5px;
	--MillenniumSpacing-Normal: 10px;
	--MillenniumSpacing-Large: 20px;

	/* Match Steam's .DialogButton border-radius */
	--MillenniumControls-BorderRadius: 2px;
	--MillenniumControls-IconSize: 16px;
}

.MillenniumButtonsSection {
    display: flex;
    flex-wrap: wrap;
    gap: var(--MillenniumSpacing-Normal);
    margin-top: var(--MillenniumSpacing-Normal);

	.DialogButton {
		width: unset;
		flex-grow: 1;
	}
}

.MillenniumLogsSection .DialogButton {
	width: -webkit-fill-available;
}

.MillenniumButton {
	display: flex !important;
	align-items: center !important;
	justify-content: center !important;
	gap: var(--MillenniumSpacing-Normal) !important;

	svg {
		width: var(--MillenniumControls-IconSize);
		height: var(--MillenniumControls-IconSize);
	}
}

/* Inherits .MillenniumButton */
.MillenniumIconButton {
	--size: 32px;
	padding: 0 !important;
	width: var(--size) !important;
	height: var(--size) !important;
}

.MillenniumColorPicker {
	--size: 34px;
	background: transparent;
	border: none;
	padding: 0;
	/* Align with DialogButton */
	margin-block: 2px;
	flex-shrink: 0;
	width: var(--size);
	height: var(--size);

	&::-webkit-color-swatch-wrapper {
		padding: 0;
	}

	&::-webkit-color-swatch {
		border: none;
		border-radius: var(--MillenniumControls-BorderRadius);
	}
}

/**
 * Placeholder
 */
.MillenniumPlaceholder_Container {
	gap: var(--MillenniumSpacing-Normal);
	width: 100%;
	height: 100%;
	display: flex;
	flex-direction: column;
	align-items: center;
	justify-content: center;
	text-align: center;
}

.MillenniumPlaceholder_Icon {
	width: 64px;
}

.MillenniumPlaceholder_Header {
	color: var(--MillenniumTextColor-Normal);
	font: var(--MillenniumText-HeadingMedium);
}

.MillenniumPlaceholder_Text {
	color: var(--MillenniumTextColor-Muted);
	font: var(--MillenniumText-BodyLarge);
}

.MillenniumPlaceholder_Buttons {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
}

/* Override Steam styles */
.MillenniumSettings {
	/* <SidebarNavigation> is not supposed to be in the main window, so add a
	 * border to distinguish it from the nav bar. */
	border-top: 1px solid rgba(61, 68, 80, .65);
	min-height: 100% !important;

	.DialogContent_InnerWidth {
		max-width: unset !important;
	}

	.DialogContentTransition {
		max-width: unset !important;
	}

	/* Fix the dropdown not filling the proper width when specific theme names are too long. */
	.DialogDropDown {
		min-width: max-content !important;
	}

	textarea.DialogInput {
		width: 100% !important;
	}

	.PageListColumn {
		min-height: unset !important;
	}

	.${fieldClasses.FieldChildrenInner} {
		gap: var(--MillenniumSpacing-Normal);
		align-items: center;
	}

	.${pagedSettingsClasses.PageListItem_Title} {
		overflow: visible !important;
		flex-grow: 1;
	}

	.sideBarUpdatesItem {
		display: flex;
		gap: var(--MillenniumSpacing-Normal);
		justify-content: space-between;
		align-items: center;
		overflow: visible !important;
	}

	.FriendMessageCount {
		display: flex !important;
		margin-top: 0px !important;
		position: initial !important;

		line-height: 20px;
		height: fit-content !important;
		width: fit-content !important;
	}
}

/**
 * Logs
 */
.MillenniumLogs_LogItemButton {
	&:not([data-warning-count="0"]) svg {
		color: var(--MillenniumTextColor-Warning);
	}

	&:not([data-error-count="0"]) svg {
		color: var(--MillenniumTextColor-Error);
	}

	/* Nothing to display */
	&[data-warning-count="0"][data-error-count="0"] > .tool-tip-source {
		display: none;
	}

	& > .tool-tip-source {
		display: flex;
	}
}

.MillenniumLogs_HeaderTextTypeContainer {
	display: flex;
    flex-direction: column;
    justify-content: center;
}

.MillenniumLogs_HeaderTextTypeCount {
	color: var(--MillenniumTextColor-Muted);
	font: var(--MillenniumText-BodySmall);

	&[data-type="error"]:not([data-count="0"]) {
		color: var(--MillenniumTextColor-Error);
	}

	&[data-type="warning"]:not([data-count="0"]) {
		color: var(--MillenniumTextColor-Warning);
	}
}

.MillenniumLogs_TextContainer {
	gap: var(--MillenniumSpacing-Large);
	margin-top: var(--MillenniumSpacing-Small);
	display: flex;
	flex-direction: column;
	height: -webkit-fill-available;
}

.MillenniumLogs_TextControls {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
	justify-content: space-between;
}

.MillenniumLogs_ControlSection {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
	justify-content: space-between;
}

.MillenniumLogs_NavContainer {
    display: flex;
    gap: var(--MillenniumSpacing-Normal);
}

.MillenniumLogs_Icons {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
}

.MillenniumLogs_Text {
	color: inherit;
	line-height: inherit;
	padding: var(--MillenniumSpacing-Normal);
	margin: 0;
	overflow-y: auto;
	white-space: pre-wrap;
	height: 100%;
	user-select: text;
	font-family: Consolas, "Courier New", monospace;
}

/**
 * Plugins
 */
.MillenniumPlugins_PluginLabel,
.MillenniumThemes_ThemeLabel {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
	align-items: center;
}

.MillenniumItem_Version {
	color: #8b929a;
	font: var(--MillenniumText-BodySmall);
	margin-left: var(--MillenniumSpacing-Normal);
}

/**
 * Themes
 */
.MillenniumThemes_AccentColorField[data-default-color="true"] {
	.DialogButton {
		display: none;
	}
}

/**
 * Updates
 */
.MillenniumUpdates_Description {
    display: flex;
    flex-direction: column;
    overflow: hidden;
    transition: all 0.5s ease;
}

.MillenniumUpdates_Field[data-expanded="false"] {
	.MillenniumUpdates_ExpandButton > svg {
		transform: rotate(180deg);
	}

	.MillenniumUpdates_Description {
		height: 0 !important;
	}
}

.MillenniumUpdates_ProgressBar {
	/* <Field> override */
	align-self: baseline;

	&:not([role="progressbar"]) {
		padding: 0 !important;
		/* icon button size + .DialogButton margin-block * 2 */
		height: calc(32px + 2px * 2) !important;

		&::after {
			content: unset !important;
		}
	}
}

/**
 * Dialogs
 */
.MillenniumInstallerDialog {
	width: 450px;
}

.MillenniumInstallerDialog_ProgressBar div {
	transition: all 0.5s ease 0s !important;
}

.MillenniumInstallerDialog_ProgressBar {
	&::after {
		content: unset !important;
	}

	* { 
		width: 100%; 
		text-align: right;
	}
}

.MillenniumInstallDialog_TutorialImage {
	margin-block: var(--MillenniumSpacing-Normal);
	width: 100%;
}

/**
 * Other
 */
.MillenniumPluginSettingsGrid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: var(--MillenniumSpacing-Large);
    padding: 2ex 5ex 2ex 5ex;
}

._1aw7cA3mAZfWt8idAlVJWi:has(.SliderControlPanelGroup) {
	width: -webkit-fill-available;
}

.MillenniumPluginSettingsSliderValue {
    position: absolute;
    right: 0;
    top: 5px;
    font-size: 14px;
}

.MillenniumPlaceholder_Button {
	min-width: fit-content;
}
`;

export const MillenniumDesktopSidebarStyles = ({
	openAnimStart,
	isDesktopMenuOpen,
	isViewingPlugin,
}: {
	openAnimStart: boolean;
	isDesktopMenuOpen: boolean;
	isViewingPlugin: boolean;
}) => {
	const styles = `
    .title-area { 
      	z-index: 999999 !important; 
    }

    .MillenniumDesktopSidebar {
		--sidebar-width: 350px;
		position: absolute;
		height: 100%;
		width: var(--sidebar-width);
		top: 0px;
		right: 0px;
		z-index: 999;
		transition: transform 0.4s cubic-bezier(0.65, 0, 0.35, 1);
		transform: ${openAnimStart ? 'translateX(0px)' : 'translateX(calc(var(--sidebar-width) + 16px))'};
		overflow-y: auto;
		display: ${isDesktopMenuOpen ? 'flex' : 'none'};
		flex-direction: column;
		background: #171d25;
    }

	.MillenniumDesktopSidebar_Content {
		padding: ${isViewingPlugin ? '16px 20px 0px 20px' : '16px 0 0 0'};
		display: flex;
		flex-direction: column;
		height: 100%;
	}

    .MillenniumDesktopSidebar_Overlay {
		position: absolute;
		inset: 0;
		z-index: 998;
		/* Match .ModalOverlayBackground */
		background: rgba(0, 0, 0, 0.8);
		opacity: ${openAnimStart ? 1 : 0};
		display: ${isDesktopMenuOpen ? 'flex' : 'none'};
		transition: opacity 0.4s cubic-bezier(0.65, 0, 0.35, 1);
    }

	.MillenniumDesktopSidebar_Title {
		/* Use the top bar's height, since -webkit-app-region is for some reason
		 * unreliable (but set it anyway) here when it touches the top bar, not
		 * letting us press the button but drag the window instead. */
		padding-block-start: 65px;
		padding-inline: 16px;
		position: sticky;
		top: 0;
		-webkit-app-region: no-drag;
	}
    `;

	return <style>{styles}</style>;
};

const Styles = () => <style>{styles}</style>;

export default Styles;
