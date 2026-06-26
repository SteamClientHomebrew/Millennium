/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

import { ConfirmModalProps, ShowModalResult, SingleDropdownOption } from '@steambrew/client';

export interface Patch {
	MatchRegexString: string;
	TargetCss?: string | Array<string>;
	TargetJs?: string | Array<string>;
}

export interface FundingTypes {
	kofi: string;
}

export interface GitInfo {
	owner: string;
	repo_name: string;
}

export interface ConditionalPatch {
	/**
	 * A list of regex strings to match against the current window.
	 * ex: ["^Steam$", ".*"]
	 */
	affects: string[];
	src: string;
}

export const CommonPatchTypes = ['TargetCss', 'TargetJs'];

export enum ConditionalControlFlowType {
	TargetCss,
	TargetJs,
}

export interface ConditionalControlFlow {
	TargetCss?: ConditionalPatch;
	TargetJs?: ConditionalPatch;
}

export interface Conditions {
	[settingName: string]: ICondition;
}

export interface SliderConfig {
	min: number;
	max: number;
	step?: number;
	unit?: string;
	cssVariable: string;
}

export interface ICondition {
	default?: string;
	description?: string;
	/**
	 * Tab to put the condition in.
	 *
	 * If no condition uses them, there will be no tabs.
	 * If some conditions include it, those who don't will be put in a separate tab.
	 */
	tab?: string;
	slider?: SliderConfig;
	section?: string;
	values?: {
		[condition: string]: ConditionalControlFlow;
	};
}

export interface ConditionsStore {
	[settingName: string]: string;
}

/**
 * High-level abstraction derived from the backend.
 * There is no guarantee a given Theme contains any elements
 */
export interface Theme {
	RootColors?: string; // path to root colors
	Patches?: Patch[];
	UseDefaultPatches?: boolean;
	Conditions?: Conditions;
	author?: string;
	description?: string;
	funding?: FundingTypes;
	github?: GitInfo;
	header_image?: URL;
	splash_image?: URL;
	name?: string;
	version?: string;
	source?: string;
	tags?: string[];
}

/**
 * Wrapped return from backend to interpolate foldername, and theme config
 */
export interface ThemeItem {
	data: Theme;
	native: string;
	// failed to parse the current theme
	failed: boolean;
}

/**
 * MultiDropdownOption extended to hold theme data
 */
export interface ComboItem extends SingleDropdownOption {
	theme: ThemeItem;
}

export interface Plugin {
	common_name?: string;
	name: string;
	description?: string;
	version?: string;
	useBackend?: boolean;
	backendType?: string;
	__private_browser_extension?: boolean;
}

export type PluginStatus = 'running' | 'disabled' | 'crashed';

export interface PluginCrashInfo {
	plugin: string;
	displayName?: string;
	exitCode: number;
	crashDumpDir?: string;
}

export interface PluginComponent {
	path: string;
	enabled: boolean;
	status: PluginStatus;
	crash?: PluginCrashInfo;
	data: Plugin;
	isChromeExtension?: boolean;
}

export interface PluginMetrics {
	name: string;
	rss_bytes: number;
	heap_bytes: number;
	cpu_percent: number;
}

/**
 * SystemAccentColor, High-level abstraction derived from the backend.
 */
export interface SystemAccentColor {
	accent: string;
	accentRgb: string;
	light1: string;
	light1Rgb: string;
	light2: string;
	light2Rgb: string;
	light3: string;
	light3Rgb: string;
	dark1: string;
	dark1Rgb: string;
	dark2: string;
	dark2Rgb: string;
	dark3: string;
	dark3Rgb: string;
	originalAccent: string;
}

export interface UpdateItem {
	message: string;
	date: string;
	commit: string;
	native: string;
	name: string;
}

/** Plugin update entry returned by the steambrew updates API. */
export interface PluginUpdateInfo {
	id: string;
	name: string;
	pluginDirectory: string;
	hasUpdate?: boolean;
	pluginInfo?: {
		commitId?: string;
		pluginJson?: { name: string; common_name?: string };
		[key: string]: any;
	};
	[key: string]: any;
}

/** Response shape for `Core_GetUpdates`. Either side can also degrade to `{ error }`. */
export interface UpdatesResponse {
	themes?: UpdateItem[] | { error: string };
	plugins?: PluginUpdateInfo[] | { error: string };
}

/** Generic `{success, error/message}` result returned by uninstall/setCondition handlers. */
export interface BackendOpResult {
	success: boolean;
	message?: string;
	error?: string;
}

/** Response shape for backgrounded install operations (`Core_InstallTheme` / `Core_InstallPlugin`). */
export interface InstallStartResponse {
	success: boolean;
	started?: boolean;
	opId?: number;
	message?: string;
	error?: string;
}

/** Response shape for backgrounded update operations. */
export interface OpIdResponse {
	opId: number;
}

export enum ColorType {
	RawRGB = 1,
	RGB = 2,
	RawRGBA = 3,
	RGBA = 4,
	Hex = 5,
	Unknown = 6,
}

/** A single color editable in the theme color picker, returned by `Core_GetThemeColorOptions`. */
export interface ThemeColorOption {
	name?: string;
	description?: string;
	color: string;
	type: ColorType;
	hex: string;
	defaultColor: string;
}

/** Shape of `m_millennium_updater->has_any_updates()` (sent inside SettingsProps). */
export interface MillenniumUpdates {
	hasUpdate: boolean;
	updateInProgress: boolean;
	newVersion: GitHubRelease | null;
	platformRelease: GitHubReleaseAsset | null;
}

export interface Settings {
	active: string;
	scripts: boolean;
	styles: boolean;
	updateNotifications: boolean;
}

export interface SettingsProps {
	accent_color: SystemAccentColor;
	active_theme: ThemeItem;
	conditions: ConditionsStore;
	settings: Settings;
	steamPath: string;
	installPath: string;
	themesPath: string;
	millenniumVersion: string;
	enabledPlugins: string;
	millenniumUpdates?: MillenniumUpdates;

	buildDate: string;
	gitCommitOid: string;
	platformType: OSType;
	millenniumLinuxUpdateScript?: string;
	quickCss: string;
	pendingCrashes?: PluginCrashInfo[];
}

export interface ColorProp {
	ColorName: string;
	Description: string;
	HexColorCode: string;
	OriginalColorCode: string;
}

export interface ThemeItemV1 extends Theme {
	GlobalsColors: ColorProp[];
}

export enum UpdaterOptionProps {
	UNSET,
	YES,
	NO,
}

export enum OSType {
	Windows,
	Linux,
	Darwin, // macOS
}

export interface IProgressProps {
	progress: number;
	status: string;
	isComplete: boolean;
	success?: boolean;
	opId?: number;
}

export interface InstallerProps {
	updateInstallerState: (element: React.ReactElement) => void;
	ShowMessageBox: (message: React.ReactNode, title: React.ReactNode, props?: ConfirmModalProps) => void | Promise<unknown>;
	modal: ShowModalResult;
	refetchDataCb?: () => void;
}

export interface GitHubRelease {
	url: string;
	assets_url: string;
	upload_url: string;
	html_url: string;
	id: number;
	node_id: string;
	tag_name: string;
	target_commitish: string;
	name: string;
	draft: boolean;
	prerelease: boolean;
	created_at: string;
	published_at: string;
	tarball_url: string;
	zipball_url: string;
	body: string | null;
	author: {
		login: string;
		id: number;
		node_id: string;
		avatar_url: string;
		gravatar_id: string;
		url: string;
		html_url: string;
		followers_url: string;
		following_url: string;
		gists_url: string;
		starred_url: string;
		subscriptions_url: string;
		organizations_url: string;
		repos_url: string;
		events_url: string;
		received_events_url: string;
		type: string;
		site_admin: boolean;
	};
	assets: GitHubReleaseAsset[];
}

export interface GitHubReleaseAsset {
	url: string;
	id: number;
	node_id: string;
	name: string;
	label: string | null;
	uploader: {
		login: string;
		id: number;
		node_id: string;
		avatar_url: string;
		gravatar_id: string;
		url: string;
		html_url: string;
		followers_url: string;
		following_url: string;
		gists_url: string;
		starred_url: string;
		subscriptions_url: string;
		organizations_url: string;
		repos_url: string;
		events_url: string;
		received_events_url: string;
		type: string;
		site_admin: boolean;
	};
	content_type: string;
	state: string;
	size: number;
	download_count: number;
	created_at: string;
	updated_at: string;
	browser_download_url: string;
}

export enum OnMillenniumUpdate {
	DO_NOTHING = 0,
	NOTIFY = 1,
	AUTO_INSTALL = 2,
}

export enum MillenniumUpdateChannel {
	STABLE = 'stable',
	BETA = 'beta',
}
