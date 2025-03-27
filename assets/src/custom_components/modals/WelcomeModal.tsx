import {
	callable,
	DialogBody,
	DialogBodyText,
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
import { steamSuperNavClass } from '../../classes';
import { Separator } from '../../components/ISteamComponents';

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

	const millenniumContextMenuItem: HTMLElement = Array.from(await Millennium.findElement(steamSuperNavDropdown.document, '.contextMenuItem')).find(
		(element: Element) => (element as HTMLElement).textContent === 'Millennium',
	) as HTMLElement;

	// Color it yellow to highlight it
	millenniumContextMenuItem.style.setProperty('color', '#d04dd0');
	millenniumContextMenuItem.style.setProperty('font-weight', '700');

	// Remove it after its hovered.
	millenniumContextMenuItem?.addEventListener('mouseover', () => {
		millenniumContextMenuItem.style.removeProperty('color');
		millenniumContextMenuItem.style.removeProperty('font-weight');
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
				<DialogHeader> Welcome to Millennium 👋 </DialogHeader>
				<DialogBody>
					<DialogBodyText>
						<p>
							We're excited for you to be apart of the community! Since this is your first time launching, it’s the perfect opportunity
							to introduce you to a few things.
						</p>
						<Separator />

						<p>
							If you’re not familiar yet, most of Millennium's settings can be accessed{' '}
							<b>
								<a href="#" style={{ color: '#d04dd0' }} onClick={OpenMillenniumSettings}>
									here
								</a>
							</b>
							. You can also find them under{' '}
							<b>
								<a href="#" style={{ color: '#d04dd0' }} onClick={HighlightMillenniumSettings}>
									Steam → Millennium Settings
								</a>
							</b>
							.
						</p>
						<p>
							From there, you'll find directions on where to install themes/plugins, how to enable/disable certain features, and more.
							<br />
							And remember, you can always{' '}
							<b>
								<a href="#" style={{ color: '#d04dd0' }} onClick={OpenDocumentation}>
									view our documentation
								</a>
							</b>{' '}
							if you have any questions.
						</p>
					</DialogBodyText>

					<DialogFooter>
						<div className="DialogTwoColLayout _DialogColLayout Panel">
							<DialogButton
								className="Secondary"
								onClick={() => {
									resolve(welcomeModalWindow.Close());
								}}
							>
								Continue
							</DialogButton>
						</div>
					</DialogFooter>
				</DialogBody>
			</GenericConfirmDialog>
		);

		welcomeModalWindow = showModal(<RenderWelcomeModal />, pluginSelf.mainWindow, {
			strTitle: 'Welcome to Millennium!',
			popupHeight: 475,
			popupWidth: 625,
		});
	});
