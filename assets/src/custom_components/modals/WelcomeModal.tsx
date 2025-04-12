import {
	callable,
	DialogBody,
	DialogButton,
	DialogFooter,
	DialogHeader,
	Millennium,
	pluginSelf,
	showModal,
	ShowModalResult,
} from '@steambrew/client';
import { GenericConfirmDialog } from '../GenericDialog';
import { MillenniumSettings } from '../SettingsModal';
import { contextMenuClasses, steamSuperNavClass } from '../../classes';
import { Separator } from '../../components/ISteamComponents';
import Styles from '../../styles';

const OpenMillenniumSettings = () => {
	showModal(<MillenniumSettings />, pluginSelf.mainWindow, {
		strTitle: 'Millennium',
		popupHeight: 675,
		popupWidth: 850,
	});
};

const FindWindow = (strName: string): Window | undefined => {
	// @ts-ignore
	const [_, windowInfo] = Array.from<any>(g_PopupManager?.m_mapPopups?.data_ || []).find(
		([_, value]) => value?.value_?.m_strName === strName || value?.value_?.m_strTitle === strName,
	);

	return windowInfo?.value_?.m_popup?.window;
};

const HighlightMillenniumSettings = async () => {
	const steamSuperNavDropdown = FindWindow('Steam Root Menu');
	console.log(steamSuperNavDropdown);

	const millenniumContextMenuItem: HTMLElement = Array.from(
		await Millennium.findElement(steamSuperNavDropdown.document, '.MillenniumContextMenuItem'),
	)[0] as HTMLElement;
	const activeClassName = contextMenuClasses.active;

	// Make it hover-styled to highlight it
	millenniumContextMenuItem.classList.add(activeClassName);

	// Remove it after its hovered.
	millenniumContextMenuItem?.addEventListener('mouseover', () => {
		millenniumContextMenuItem.classList.remove(activeClassName);
	});

	const mainSteamWindow = FindWindow('SP Desktop_uid0');
	(mainSteamWindow as any).SteamClient.Window.BringToFront();

	setTimeout(() => {
		const steamSuperNavButton = mainSteamWindow.document.querySelector('.' + steamSuperNavClass) as HTMLElement;
		steamSuperNavButton?.click();
	}, 100);
};

const OpenDocumentation = () => {
	SteamClient.System.OpenInSystemBrowser('https://docs.steambrew.app');
};

const SetConfig = callable<[{ header: string; key: string; value: string }], void>('IniConfig.set_config');
const GetConfig = callable<[{ header: string; key: string; fallback: string }], string>('IniConfig.get_config');

export const ShowWelcomeModal = async () =>
	new Promise(async (resolve) => {
		const hasShownWelcomeModal = await GetConfig({ header: 'Settings', key: 'has_shown_welcome_modal', fallback: 'no' });

		if (hasShownWelcomeModal === 'yes') {
			return resolve(undefined);
		} else {
			SetConfig({ header: 'Settings', key: 'has_shown_welcome_modal', value: 'yes' });
		}

		let welcomeModalWindow: ShowModalResult;

		const RenderWelcomeModal = () => (
			<GenericConfirmDialog>
				<Styles />
				<DialogHeader> Welcome to Millennium 👋 </DialogHeader>
				<DialogBody className="MillenniumGenericDialog_DialogBody">
					<div>
						We're excited for you to be a part of the community! Since this is your first time launching, it’s the perfect opportunity to
						introduce you to a few things.
					</div>
					<Separator />

					<div>
						If you’re not familiar yet, most of Millennium's settings can be accessed{' '}
						<a href="#" onClick={OpenMillenniumSettings}>
							here
						</a>
						. You can also find them under{' '}
						<a href="#" onClick={HighlightMillenniumSettings}>
							Steam → Millennium Settings
						</a>
						.
					</div>
					<div>
						From there, you'll find directions on where to install themes/plugins, how to enable/disable certain features, and more.
						<br />
						And remember, you can always{' '}
						<a href="#" onClick={OpenDocumentation}>
							view our documentation
						</a>{' '}
						if you have any questions.
					</div>
				</DialogBody>
				<DialogFooter>
					<div className="DialogTwoColLayout _DialogColLayout Panel">
						<DialogButton
							onClick={() => {
								resolve(welcomeModalWindow.Close());
							}}
						>
							Continue
						</DialogButton>
					</div>
				</DialogFooter>
			</GenericConfirmDialog>
		);

		welcomeModalWindow = showModal(<RenderWelcomeModal />, pluginSelf.mainWindow, {
			strTitle: 'Welcome to Millennium!',
			popupHeight: 475,
			popupWidth: 625,
		});
	});
