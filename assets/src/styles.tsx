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
	/* Base for these: hsl(41 75% 50% / 1) */
	--MillenniumTextColor-Success: #4fdf20;
	--MillenniumTextColor-Error: #df4020;
	--MillenniumTextColor-Warning: #d3df20;

	--MillenniumSpacing-Small: 5px;
	--MillenniumSpacing-Normal: 10px;
	--MillenniumSpacing-Large: 20px;

	/* Match Steam's DialogButton border-radius */
	--MillenniumControls-BorderRadius: 2px;
	--MillenniumControls-IconSize: 16px;
}

.MillenniumButtonsSection {
    gap: var(--MillenniumSpacing-Normal);
    display: flex;
    flex-wrap: wrap;
    flex-direction: column;
    margin-top: var(--MillenniumSpacing-Small);

	.DialogButton {
		width: unset;
	}
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

.MillenniumIconButton {
	margin: 0 !important;
	display: flex !important;
	align-items: center;

	svg {
		width: var(--MillenniumControls-IconSize);
		height: var(--MillenniumControls-IconSize);
	}
}

/* Override Steam styles */
.MillenniumSettings {
	min-height: unset !important;

	.DialogContent_InnerWidth {
		max-width: 620px !important;
	}

	/* Fix the dropdown not filling the proper width when specific theme names are too long. */
	.DialogDropDown {
		min-width: max-content !important;
	}

	textarea.DialogInput {
		width: 100% !important;
	}

	.${fieldClasses.FieldChildrenInner} {
		gap: var(--MillenniumSpacing-Normal);
	}

	img[alt="Steam Spinner"] {
		width: var(--MillenniumControls-IconSize);
		height: var(--MillenniumControls-IconSize);

		& + div {
			display: none;
		}
	}
}

/**
 * Bug Report
 */
.MillenniumBugReport_SubmitButton {
	width: unset;
}

/**
 * Logs
 */
.MillenniumLogs_Header {
	gap: var(--MillenniumSpacing-Normal);
	/* Match <Field> padding */
	margin-top: var(--MillenniumSpacing-Normal);
	margin-bottom: var(--MillenniumSpacing-Large);
	display: flex;
	align-items: center;
	justify-content: space-between;
}

.MillenniumLogs_HeaderNav {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
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
    gap: 10px;
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
.MillenniumPlugins_PluginLabel {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
	align-items: center;
}

.MillenniumPlugins_StatusDot {
	--size: 8px;
	border-radius: 50%;
	display: inline-block;
	width: var(--size);
	height: var(--size);

	&[data-type="success"] {
		background-color: var(--MillenniumTextColor-Success);
	}

	&[data-type="error"] {
		background-color: var(--MillenniumTextColor-Error);
	}

	&[data-type="warning"] {
		background-color: var(--MillenniumTextColor-Warning);
	}
}

.MillenniumPlugins_Version {
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

.MillenniumThemes_AccentColorUsage {
	&[data-accent-color-in-use="false"] {
		color: var(--MillenniumTextColor-Warning);
	}
}

/**
 * Updates
 */
.MillenniumUpdates_Label {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;
	align-items: center;
}

.MillenniumUpdates_LabelType {
	color: var(--MillenniumTextColor-Normal);
	font: var(--MillenniumText-BodySmall);
	border-radius: var(--MillenniumControls-BorderRadius);
	padding: var(--MillenniumSpacing-Small);
	width: 45px;
    text-align: center;

	&[data-type="theme"] {
		background-color: #007eff;
	}

	&[data-type="plugin"] {
		background-color: #564688;
	}
}

.MillenniumUpdates_Description {
	display: flex;
	flex-direction: column;
}

.MillenniumUpdates_ThemeButton {
	min-width: 80px;
}

.MillenniumUpToDate_Container, .MillenniumErrorModal_Container {
	gap: var(--MillenniumSpacing-Large);
	height: 100%;
	display: flex;
	flex-direction: column;
	align-items: center;
	justify-content: center;
	margin-left: 100px;
    margin-right: 100px;
}

.MillenniumUpToDate_Header, .MillenniumErrorModal_Header {
	color: var(--MillenniumTextColor-Normal);
	font: var(--MillenniumText-BodyLarge);
}

.MillenniumUpToDate_Text, .MillenniumErrorModal_Text {
	color: var(--MillenniumTextColor-Muted);
	font: var(--MillenniumText-BodyMedium);
	text-align: center;
}

button.MillenniumErrorModal_Button.DialogButton {
    width: fit-content;
    padding: 0px 50px;
}

/**
 * Dialogs
 */
.MillenniumGenericDialog_DialogBody {
	gap: var(--MillenniumSpacing-Large);
}

.MillenniumAboutTheme_Version {
	color: var(--MillenniumTextColor-Muted);
	font: var(--MillenniumText-HeadingSmall);
	margin-left: var(--MillenniumSpacing-Small);
}

.MillenniumRelease_DialogBody {
	.DialogBodyText {
		margin: 0;
		overflow-y: scroll;
	}

	.DialogFooter {
		flex-direction: column;
		gap: var(--MillenniumSpacing-Normal);
	}

	.DialogTwoColLayout {
		justify-content: space-between;
	}
}

.MillenniumRelease_UpdateLinksContainer {
	gap: var(--MillenniumSpacing-Normal);
	display: flex;

	a {
		font: var(--MillenniumText-BodyMedium);

		&:hover {
			cursor: pointer;
		}
	}
}

.MillenniumDownloadInfo_Field {
	gap: var(--MillenniumSpacing-Small);
	display: flex;
    justify-content: space-between;
    flex-wrap: wrap;
    margin-top: var(--MillenniumSpacing-Small);
}

.MillenniumDownloadInfo_Publisher {
	gap: var(--MillenniumSpacing-Small);
	display: flex;
	align-items: center;
}

.MillenniumDownloadInfo_PublisherAvatar {
	--size: 20px;
	width: var(--size);
	height: var(--size);
}

.MillenniumSelectUpdate_FooterInfo {
	color: var(--MillenniumTextColor-Muted);
	font: var(--MillenniumText-BodySmall);
}

.MillenniumSelectUpdate_SecurityWarning {
	color: var(--MillenniumTextColor-Warning);
}

button.DialogButton.MillenniumIconButton {
    width: fit-content;
    gap: var(--MillenniumSpacing-Normal);
    display: flex;
    align-items: center;
}
`;

const Styles = () => <style>{styles}</style>;

export default Styles;
