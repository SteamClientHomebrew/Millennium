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
	}

	.MillenniumButtonsSection {
		gap: var(--MillenniumSpacing-Normal);
		display: flex;
		flex-wrap: wrap;

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
		/* Force proper styling on buttons next to dropdowns. */
		display: flex;
		width: auto;
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
		margin-top: var(--MillenniumSpacing-Large);
		display: flex;
		flex-direction: column;
	}

	.MillenniumLogs_TextControls {
		gap: var(--MillenniumSpacing-Normal);
		display: flex;
		justify-content: space-between;
	}

	.MillenniumLogs_Icons {
		gap: var(--MillenniumSpacing-Normal);
		display: flex;
	}

	.MillenniumLogs_IconButton {
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
	}

	/**
	 * Plugins
	 */
	.MillenniumPlugins_PluginLabel {
		gap: var(--MillenniumSpacing-Small);
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
			background-color: var(--MillenniumTextColor-success);
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
		background-color: #007eff;
		color: var(--MillenniumTextColor-Normal);
		font: var(--MillenniumText-BodySmall);
		border-radius: var(--MillenniumControls-BorderRadius);
		padding: var(--MillenniumSpacing-Small);
	}

	.MillenniumUpdates_Description {
		display: flex;
		flex-direction: column;
	}

	.MillenniumUpdates_ThemeButton {
		min-width: 80px;
	}

	.MillenniumUpToDate_Container {
		gap: var(--MillenniumSpacing-Large);
		height: 100%;
		display: flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
		height: 100%;
	}

	.MillenniumUpToDate_Header {
		color: var(--MillenniumTextColor-Normal);
		font: var(--MillenniumText-HeadingLarge);
		margin-top: -120px;
	}

	.MillenniumUpToDate_Text {
		color: var(--MillenniumTextColor-Normal);
		font: var(--MillenniumText-BodyLarge);
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
		}

		.DialogTwoColLayout {
			align-items: center;
			justify-content: space-between;
		}
	}

	.MillenniumRelease_UpdateLinksContainer {
		gap: var(--MillenniumSpacing-Normal);
		display: flex;
	}

	.MillenniumDownloadInfo_Field {
		gap: var(--MillenniumSpacing-Small);
		display: flex;
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
`;

// 0 idea if I can use my own rollup plugins for CSS importing, so fuck it
const Styles = () => <style>{styles}</style>;

export default Styles;
