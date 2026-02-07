import { useEffect, useMemo, useRef, useState } from 'react';
import { EditorView, basicSetup } from 'codemirror';
import { css } from '@codemirror/lang-css';
import { vscodeDark } from '@uiw/codemirror-theme-vscode';
import {
	callable,
	Classes,
	DialogBody,
	DialogButton,
	DialogControlsSection,
	DialogHeader,
	Field,
	findModuleDetailsByExport,
	IconsModule,
	pluginSelf,
} from '@steambrew/client';
import { ViewUpdate } from '@codemirror/view';
import ReactDOM from 'react-dom';
import {
	contextMenuManager,
	createWindowContext,
	g_ModalManager,
	GenericConfirmDialog,
	ModalManagerInstance,
	OwnerWindowRef,
	PopupModalHandler,
	popupModalManager,
	RenderWindowTitle,
	SettingsDialogSubHeader,
	windowHandler,
} from '../../components/SteamComponents';
import { settingsClasses } from '../../utils/classes';
import { setEditorCode, setIsMillenniumOpen, useQuickCssState } from '../../utils/quick-css-state';
import { locale } from '../../utils/localization-manager';
import Styles from '../../utils/styles';

function UpdateStylesLive(cssContent: string) {
	for (const popup of g_PopupManager.GetPopups()) {
		const quickCssStyles = popup?.window?.document?.getElementById('MillenniumQuickCss');
		if (quickCssStyles) {
			quickCssStyles.innerHTML = cssContent;
		}
	}
}

export async function LoadStylesFromDisk(): Promise<string> {
	return JSON.parse(await callable<[], string>('Core_LoadQuickCss')());
}

export function SaveStylesToDisk(cssContent: string) {
	callable<[{ css: string }]>('Core_SaveQuickCss')({ css: cssContent });
	pluginSelf.quickCSS = cssContent;
	UpdateStylesLive(cssContent);
}

export const MillenniumQuickCssEditor = () => {
	const g_ModalManagerInstance = g_ModalManager();
	const { editorCode } = useQuickCssState();

	const targetBrowserProps = {
		m_unPID: 0,
		m_nBrowserID: -1,
		m_unAppID: 0,
		m_eUIMode: 7,
	};

	const windowDimensions = {
		dimensions: { width: 650, height: 400, left: 0, top: 0 },
		minWidth: 200,
		minHeight: 100,
	};

	const windowConfig = {
		title: locale.quickCssEditorTitle,
		body_class: 'fullheight ModalDialogBody DesktopUI',
		html_class: 'client_chat_frame fullheight ModalDialogPopup',
		popup_class: 'fullheight MillenniumQuickCss_Popup',
		strUserAgent: 'Valve Steam Client',
		eCreationFlags: 272,
		browserType: 4,
		...windowDimensions,
		owner_window: window,
		target_browser: targetBrowserProps,
		replace_existing_popup: true,
		bHideOnClose: false,
		bNoFocusOnShow: false,
		bNeverPopOut: true,
	};

	const m_contextMenuManager = useMemo(() => new contextMenuManager(), []);
	const [activeMenu, setActiveMenu] = useState(m_contextMenuManager.m_ActiveMenu);
	const { popup, element } = createWindowContext('MillenniumDesktopPopup', windowConfig, windowHandler('Window_MillenniumDesktop'));

	useEffect(() => {
		m_contextMenuManager.OnMenusChanged.m_vecCallbacks.push(
			(() => {
				const v_callbackFn = () => setActiveMenu(m_contextMenuManager.m_ActiveMenu);
				v_callbackFn.Dispatch = v_callbackFn;
				return v_callbackFn;
			})(),
		);
	}, [m_contextMenuManager]);

	useEffect(() => {
		if (!popup) {
			return;
		}

		ModalManagerInstance.RegisterModalManager(g_ModalManagerInstance, popup);
		popupModalManager.SetMenuManager(popup, m_contextMenuManager);
	}, [popup]);

	const dialogProps = {
		bOverlapHorizontal: true,
		bMatchWidth: false,
		bFitToWindow: true,
		strClassName: 'DialogMenuPosition',
	};

	const editorRef = useRef(null);
	const containerRef = useRef(null);

	useEffect(() => {
		if (containerRef.current && !editorRef.current) {
			editorRef.current = new EditorView({
				doc: editorCode,
				extensions: [
					basicSetup,
					css(),
					vscodeDark,
					EditorView.updateListener.of((update: ViewUpdate) => {
						if (update.docChanged) {
							const content = update.state.doc.toString();
							SaveStylesToDisk(content);
							setEditorCode(content);
						}
					}),
				],
				parent: containerRef.current,
			});
		}

		return () => editorRef.current?.destroy();
	}, [element]);

	return (
		element &&
		ReactDOM.createPortal(
			<OwnerWindowRef ownerWindow={popup}>
				<div className="PopupFullWindow">
					<RenderWindowTitle popup={popup} onClose={setIsMillenniumOpen.bind(null, false)} />
					<GenericConfirmDialog>
						<DialogHeader>{locale.quickCssEditorTitle}</DialogHeader>
						<Styles />
						{/* For whatever reason only a <form> gets the full width in generic modals */}
						<form ref={containerRef} className="MillenniumQuickCss_CodeEditor" />
					</GenericConfirmDialog>
					{activeMenu && (
						<PopupModalHandler options={dialogProps} instance={activeMenu} element={activeMenu.m_position.element}>
							{activeMenu.m_rctElement}
						</PopupModalHandler>
					)}
				</div>
			</OwnerWindowRef>,
			element,
		)
	);
};

/**
 * Get statistics about the CSS content.
 * @param cssContent CSS content to analyze
 * @returns An object containing the number of rules, bytes, and kilobytes
 */
function getCssStats(cssContent: string) {
	const rules = (cssContent.match(/[^{}]*{[^}]*}/g) || []).length;
	const bytes = new TextEncoder().encode(cssContent).length;
	const kilobytes = (bytes / 1024).toFixed(2);

	return { rules, bytes, kilobytes };
}

/**
 * Quick CSS View Modal
 * @returns A React component that displays information about Quick CSS and provides a button to open the editor.
 */
export function QuickCssViewModal() {
	const { editorCode } = useQuickCssState();
	const [cssStats, setCssStats] = useState<{ rules: number; bytes: number; kilobytes: string } | null>(null);

	useEffect(() => {
		LoadStylesFromDisk().then((cssContent) => {
			setEditorCode(cssContent);
		});
	}, []);

	useEffect(() => {
		if (editorCode) {
			setCssStats(getCssStats(editorCode));
		}
	}, [editorCode]);

	return (
		<>
			<Field label={locale.quickCssDescription} icon={<IconsModule.ExclamationPoint />} bottomSeparator="none"></Field>

			{cssStats && (
				<DialogControlsSection>
					<SettingsDialogSubHeader>{locale.quickCssStatsTitle}</SettingsDialogSubHeader>
					<Field label={locale.quickCssCompiledRules} icon={<IconsModule.TextCode />}>
						{cssStats.rules || '0'}
					</Field>
					<Field label={locale.quickCssSizeOnDisk} icon={<IconsModule.HardDrive />}>
						{cssStats.kilobytes} KB ({cssStats.bytes} bytes)
					</Field>
				</DialogControlsSection>
			)}

			<Field label={locale.quickCssStatusLoaded} bottomSeparator="none" icon={<IconsModule.Checkmark color="green" />}>
				<DialogButton onClick={() => setIsMillenniumOpen(true)} className={settingsClasses.SettingsDialogButton}>
					{locale.quickCssOpenEditor}
				</DialogButton>
			</Field>
		</>
	);
}
