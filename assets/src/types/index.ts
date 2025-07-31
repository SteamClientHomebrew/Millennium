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
	values: {
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
	venv?: string;
	version?: string;
}

export interface PluginComponent {
	path: string;
	enabled: boolean;
	data: Plugin;
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
	millenniumVersion: string;
	enabledPlugins: string;
	hasCheckedForUpdates: boolean;

	updates: {
		themes?: UpdateItem[];
		plugins?: any[];
	};
	millenniumUpdates?: {
		hasUpdate: boolean;
		newVersion?: GitHubRelease;
		updateInProgress: boolean;
	};

	buildDate: string;
	platformType: OSType;
	millenniumLinuxUpdateScript?: string;
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
}

export interface InstallerProps {
	updateInstallerState: (element: React.ReactNode) => void;
	ShowMessageBox: (message: React.ReactNode, title: React.ReactNode, props?: ConfirmModalProps) => void;
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
