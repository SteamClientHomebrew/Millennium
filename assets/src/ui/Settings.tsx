import { findModuleDetailsByExport, Millennium, pluginSelf } from '@steambrew/client';
import { MillenniumSettings } from '../custom_components/SettingsModal';
import { locale } from '../locales';
import { pagedSettingsClasses } from '../classes';
import { Logger } from '../Logger';
import { useEffect, useMemo, useRef, useState, VFC } from 'react';
import ReactDOM from 'react-dom';

declare global {
	const g_PopupManager: any;
}

const settingsTabsMap = {
	themes: locale.settingsPanelThemes,
	plugins: locale.settingsPanelPlugins,
	updates: locale.settingsPanelUpdates,
	report: locale.settingsPanelBugReport,
	logs: locale.settingsPanelLogs,
	settings: locale.settingsPanelSettings,
};

export type SettingsTabs = keyof typeof settingsTabsMap;

export async function OpenSettingsTab(popup: any, activeTab: SettingsTabs) {
	if (!activeTab) {
		return;
	}

	Logger.Log(`OpenSettingsTab( '${activeTab}' )`);

	/** FIXME: Fix this to use actual router, tried it and it worked but it broke other portions of the UI. */
	const tabs = (await Millennium.findElement(popup.m_popup.document, `.${pagedSettingsClasses.PagedSettingsDialog_PageListItem}`)) as NodeListOf<HTMLElement>;
	for (const tab of tabs) {
		if (tab.textContent.includes(settingsTabsMap[activeTab])) {
			tab.click();
			break;
		}
	}
}

const createWindowContext = findModuleDetailsByExport(
	(m) =>
		m?.toString().includes('current?.params.bNoInitialShow') && m?.toString().includes('current?.params.bNoFocusOnShow') && m?.toString().includes('current.m_callbacks'),
)[1];

const ModalManagerInstance = findModuleDetailsByExport((m) => {
	return m?.hasOwnProperty('m_mapModalManager');
})[1];

const windowHandler = findModuleDetailsByExport(
	(m) => m?.toString().includes('SetCurrentLoggedInAccountID') && m?.toString().includes('bIgnoreSavedDimensions') && m?.toString().includes('updateParamsBeforeShow'),
)[1];

const RenderWindowTitle = findModuleDetailsByExport((m) => m?.toString().includes('title-bar-actions window-controls'))[1];

const g_ModalManager = findModuleDetailsByExport((m) => {
	return m.toString().includes('useContext') && m.toString().includes('ModalManager') && m.length === 0;
})[1];

const OwnerWindowRef = findModuleDetailsByExport(
	(m) =>
		m.toString().includes('ownerWindow') &&
		m.toString().includes('children') &&
		m.toString().includes('useMemo') &&
		m.toString().includes('Provider') &&
		!m.toString().includes('refFocusNavContext'),
)[1];

const popupModalManager: any = Object.values(findModuleDetailsByExport((m) => m.toString().includes('GetContextMenuManagerFromWindow'))[0]).find(
	(obj) => obj && obj.hasOwnProperty('m_mapManagers'),
);

const contextMenuManager = findModuleDetailsByExport(
	(m) =>
		m.toString().includes('CreateContextMenuInstance') &&
		m.prototype.hasOwnProperty('GetVisibleMenus') &&
		m.prototype.hasOwnProperty('GetAllMenus') &&
		m.prototype.hasOwnProperty('ShowMenu'),
)[1];

const PopupModalHandler: any = Object.values(findModuleDetailsByExport((m) => m.toString().includes('this.instance.BIsSubMenuVisible'))[0]).find(
	(obj) => obj && obj.toString().includes('this.m_elMenu.ownerDocument.defaultView.devicePixelRatio'),
);

const RenderMillennium = ({ setIsMillenniumOpen }: { setIsMillenniumOpen: React.Dispatch<React.SetStateAction<boolean>> }) => {
	const g_ModalManagerInstance = g_ModalManager();

	const targetBrowserProps = {
		m_unPID: 0,
		m_nBrowserID: -1,
		m_unAppID: 0,
		m_eUIMode: 7,
	};

	const windowDimensions = {
		dimensions: { width: 1280, height: 800, left: 0, top: 0 },
		minWidth: 1010,
		minHeight: 600,
	};

	const windowConfig = {
		title: 'Millennium',
		body_class: 'fullheight ModalDialogBody DesktopUI',
		html_class: 'client_chat_frame fullheight ModalDialogPopup',
		popup_class: 'fullheight MillenniumSettings',
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

	pluginSelf.MillenniumSettingsWindowRef = popup;

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

		popup.SteamClient.Window.BringToFront();
		ModalManagerInstance.RegisterModalManager(g_ModalManagerInstance, popup);
		popupModalManager.SetMenuManager(popup, m_contextMenuManager);
	}, [popup]);

	const dialogProps = {
		bOverlapHorizontal: true,
		bMatchWidth: false,
		bFitToWindow: true,
		strClassName: 'DialogMenuPosition',
	};

	return (
		element &&
		ReactDOM.createPortal(
			<OwnerWindowRef ownerWindow={popup}>
				<div className="PopupFullWindow">
					<RenderWindowTitle popup={popup} onClose={setIsMillenniumOpen.bind(null, false)} />
					{activeMenu && (
						<PopupModalHandler options={dialogProps} instance={activeMenu} element={activeMenu.m_position.element}>
							{activeMenu.m_rctElement}
						</PopupModalHandler>
					)}
					<MillenniumSettings />
				</div>
			</OwnerWindowRef>,
			element,
		)
	);
};

function RenderSettingsModal(_: any, retObj: any) {
	const index = retObj.props.menuItems.findIndex((prop: any) => prop.name === '#Menu_Settings');

	const [isMillenniumOpen, setIsMillenniumOpen] = useState(false);

	if (index !== -1) {
		retObj.props.menuItems.splice(index + 1, 0, {
			name: 'Millennium Settings',
			onClick: () => {
				setIsMillenniumOpen(true);
			},
			visible: true,
		});
	}

	return (
		<>
			{isMillenniumOpen && <RenderMillennium setIsMillenniumOpen={setIsMillenniumOpen} />}
			{retObj.type(retObj.props)}
		</>
	);
}

export { RenderSettingsModal };
