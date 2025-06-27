import { findClassModule } from '@steambrew/client';
import { fieldClasses } from './classes';

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
	}
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
	width: 75%;
	display: flex;
	flex-direction: column;
	align-items: center;
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
	}
}

/**
 * Logs
 */
.MillenniumLogs_LogItemButton {
	/* Nothing to display */
	&[data-warning-count="0"][data-error-count="0"] svg {
		/* Just hide to not fuck up the spacing */
		visibility: hidden;
	}

	&:not([data-warning-count="0"]) svg {
		color: var(--MillenniumTextColor-Warning);
	}

	&:not([data-error-count="0"]) svg {
		color: var(--MillenniumTextColor-Error);
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
	color: var(--MillenniumTextColor-Muted);
	font: var(--MillenniumText-BodySmall);
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
		height: 0;
	}
}

/* Actually a <Field>... */
.MillenniumUpdates_ProgressBar {
	padding: 0 !important;
	height: 32px !important;

	&::after {
		content: unset !important;
	}
}

/**
 * Dialogs
 */
.MillenniumInstallerDialog {
	width: 450px;
}

.MillenniumInstallerDialog_ProgressBar {
	&::after {
		content: unset !important;
	}

	* { 
		width: 100%; 
		text-align: start;
	}
}

.MillenniumInstallDialog_TutorialImage {
	margin-block: var(--MillenniumSpacing-Normal);
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

._2o2fXzn99OddeqZMjbDuxQ {
    display: flex;
    align-items: center;
}

.MillenniumPluginSettingsSliderValue {
    position: absolute;
    right: 0;
    top: 5px;
    font-size: 14px;
}
`;

const sidebarTitleClass = (findClassModule((m) => m.ReturnToPageListButton && m.PageListItem_Title && m.HidePageListButton) as any)?.PageListItem_Title;

const updateCountStyles = `
.PageListColumn .sideBarUpdatesItem {
	display: flex;
	gap: var(--MillenniumSpacing-Normal);
	justify-content: space-between;
	align-items: center;
	overflow: visible !important;
}

.${sidebarTitleClass} {
	overflow: visible !important;
	width: calc(100% - 32px);
}

.PageListColumn .FriendMessageCount {
	display: flex !important;
	margin-top: 0px !important;
	position: initial !important;

	line-height: 20px;
	height: fit-content !important;
	width: fit-content !important;
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
		position: absolute;
		height: 100%;
		width: 350px;
		top: 0px;
		right: 0px;
		z-index: 999;
		transition: transform 0.4s cubic-bezier(0.65, 0, 0.35, 1);
		transform: ${openAnimStart ? 'translateX(0px)' : 'translateX(366px)'};
		overflow-y: auto;
		display: ${isDesktopMenuOpen ? 'flex' : 'none'};
		flex-direction: column;
		background: #171d25;
    }

	.MillenniumDesktopSidebar_Content {
		display: flex;
		flex-direction: column;
		height: 100%;

		.MillenniumPlaceholder_Container {
			margin: auto;
		}
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
		--titlebar-height: 32px;
		--spacing: 8px;
		padding-block-start: calc(var(--titlebar-height) + var(--spacing));
		padding-inline: calc(var(--spacing) * 2);
		justify-content: space-between;
		position: sticky;
		top: 0;
		-webkit-app-region: no-drag;
	}

	.MillenniumDesktopSidebarContent {
	  	padding: ${isViewingPlugin ? '16px 20px 0px 20px' : '16px 0 0 0'};
	}
    `;

	return <style>{styles}</style>;
};

export const UpdateCountStyle = () => <style>{updateCountStyles}</style>;

const Styles = () => <style>{styles}</style>;

export default Styles;
