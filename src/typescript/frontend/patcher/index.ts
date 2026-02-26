import { Millennium, pluginSelf } from '@steambrew/client';
import { ConditionalControlFlowType as ModuleType, Patch, ThemeItem } from '../types';
import { DOMModifier, classListMatch, constructThemePath } from './Dispatch';
import { EvaluateConditions } from './v2/Conditions';
import { PatchV1, EvaluateStatements } from './v1/Conditions';
import { PatchRootMenu } from '../utils/root-menu';
import { SteamLocale } from '../utils/localization-manager';

const EvaluateModule = (module: string, type: ModuleType, document: Document) => {
	const activeTheme: ThemeItem = pluginSelf.activeTheme;
	switch (type) {
		case ModuleType.TargetCss:
			DOMModifier.AddStyleSheet(document, constructThemePath(activeTheme.native, module));
			break;
		case ModuleType.TargetJs:
			DOMModifier.AddJavaScript(document, constructThemePath(activeTheme.native, module));
			break;
	}
};

/**
 * @brief evaluates list of; or single module
 *
 * @param module module(s) to be injected into the frame
 * @param type the type of the module
 * @returns null
 */
const SanitizeTargetModule = (module: string | Array<string>, type: ModuleType, document: Document) => {
	if (module === undefined) {
		return;
	} else if (typeof module === 'string') {
		EvaluateModule(module, type, document);
	} else if (Array.isArray(module)) {
		module.forEach((node) => EvaluateModule(node, type, document));
	}
};

const EvaluatePatches = (activeTheme: ThemeItem, documentTitle: string, classList: string[], document: Document, popup: any) => {
	activeTheme.data.Patches.forEach((patch: Patch) => {
		const match = patch.MatchRegexString;
		popup.window.HAS_INJECTED_THEME = true;

		if (!documentTitle.match(match) && !classListMatch(classList, match)) {
			return;
		}

		SanitizeTargetModule(patch?.TargetCss, ModuleType.TargetCss, document);
		SanitizeTargetModule(patch?.TargetJs, ModuleType.TargetJs, document);

		// backwards compatibility with old millennium versions.
		const PatchV1 = patch as PatchV1;

		if (pluginSelf.conditionVersion == 1 && PatchV1?.Statement !== undefined) {
			EvaluateStatements(PatchV1, document);
		}
	});
};

/**
 * parses all classnames from a window and concatenates into one list
 * @param context window context from g_popupManager
 * @returns
 */
const getDocumentClassList = (context: any): string[] => {
	const bodyClass: string = context?.params?.body_class ?? String();
	const htmlClass: string = context?.params?.html_class ?? String();

	return `${bodyClass} ${htmlClass}`.split(' ').map((className: string) => '.' + className);
};

/**
 * This function adds helpful classes to the documentElement to help with CSS targeting.
 */
export function addWindowBreadcrumbs(windowContext: any) {
	function addBreadCrumb(breadcrumb: string) {
		windowContext.window.document.documentElement.classList.add(breadcrumb);
	}

	if (windowContext?.m_strName === 'SP Desktop_uid0') {
		addBreadCrumb('MillenniumWindow_MainSteamWindow');
		return;
	}

	if (windowContext?.m_strName === 'friendslist_uid0') {
		addBreadCrumb('MillenniumWindow_FriendsList');
		return;
	}

	if (windowContext?.m_strName?.includes('chat_ChatWindow_')) {
		addBreadCrumb('MillenniumWindow_FriendChatWindow');
		return;
	}

	if (windowContext?.m_strName?.includes('contextmenu')) {
		addBreadCrumb('MillenniumWindow_ContextMenu');
		addBreadCrumb((windowContext?.m_strTitle ?? '').replace(/[^a-zA-Z0-9]/g, '_'));
	}

	if (windowContext?.m_strTitle === SteamLocale('#Settings_Title')) {
		addBreadCrumb('MillenniumWindow_Settings');
		return;
	}

	/** likely the properties window of an app/game */
	if (windowContext?.m_rgParams?.minHeight === 601 && windowContext?.m_rgParams?.minWidth === 842) {
		addBreadCrumb('MillenniumWindow_GameProperties');
	}
}

export function patchDocumentContext(windowContext: any) {
	addWindowBreadcrumbs(windowContext);

	const document: Document = windowContext.window.document;

	// Append Quick CSS to global document (publicly shared)
	DOMModifier.AddStyleSheetFromText(document, pluginSelf.quickCss, 'MillenniumQuickCss');

	for (const plugin of pluginSelf.enabledPlugins) {
		document.documentElement.classList.add(plugin);
	}

	document.documentElement.setAttribute('data-millennium-plugin', pluginSelf?.enabledPlugins?.join(' '));

	if (pluginSelf.isDefaultTheme) {
		return;
	}

	const activeTheme: ThemeItem = pluginSelf.activeTheme;
	const classList: string[] = getDocumentClassList(windowContext);
	const documentTitle: string = windowContext.title;

	// Append System Accent Colors to global document (publicly shared)
	DOMModifier.AddStyleSheetFromText(document, pluginSelf.systemColor, 'SystemAccentColorInject');
	// Append old global colors struct to DOM
	pluginSelf?.GlobalsColors && DOMModifier.AddStyleSheetFromText(document, pluginSelf.GlobalsColors, 'GlobalColors');

	if (activeTheme?.data?.Conditions) {
		pluginSelf.conditionVersion = 2;
		EvaluateConditions(activeTheme, documentTitle, classList, document);
	} else {
		pluginSelf.conditionVersion = 1;
	}
	activeTheme?.data?.hasOwnProperty('Patches') && EvaluatePatches(activeTheme, documentTitle, classList, document, windowContext);

	/** Inject root colors */
	pluginSelf?.RootColors && DOMModifier.AddStyleSheetFromText(document, pluginSelf.RootColors, 'RootColors');
}

export function patchMissedDocuments() {
	for (const popup of g_PopupManager?.GetPopups()) {
		if (popup?.window?.HAS_INJECTED_THEME === undefined) {
			patchDocumentContext(popup);
		}
	}
}

export function onWindowCreatedCallback(windowContext: any): void {
	const windowTitle = windowContext.m_strTitle;

	/** Patch the steam root menu to add the Millennium root menu */
	if (windowTitle === 'Steam Root Menu') {
		PatchRootMenu();
	}

	pluginSelf.mainWindow = g_PopupManager?.GetExistingPopup?.('SP Desktop_uid0')?.m_popup?.window;

	patchMissedDocuments();
	patchDocumentContext(windowContext);
}
