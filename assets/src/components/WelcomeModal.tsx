import { ConfirmModal, pluginSelf, showModal, ShowModalResult } from '@steambrew/client';
import { settingsManager } from '../settings-manager';
import Markdown from 'markdown-to-jsx';
import { locale } from '../../locales';

export const ShowWelcomeModal = async () =>
	new Promise(async (resolve) => {
		if (settingsManager.config.misc.hasShownWelcomeModal) {
			return resolve(undefined);
		} else {
			settingsManager.config.misc.hasShownWelcomeModal = true;
			settingsManager.forceSaveConfig();
		}

		let welcomeModalWindow: ShowModalResult;

		const WelcomeModal = () => {
			return (
				<ConfirmModal
					strTitle={locale.strWelcomeModalTitle}
					strDescription={<Markdown options={{ overrides: { a: { props: { target: '_blank' } } } }}>{locale.strWelcomeModalDescription}</Markdown>}
					strOKButtonText={locale.strWelcomeModalOKButton}
					bAlertDialog={true}
					bDisableBackgroundDismiss={true}
					bHideCloseIcon={true}
					onOK={() => {
						welcomeModalWindow?.Close();
						resolve(undefined);
					}}
				/>
			);
		};

		welcomeModalWindow = showModal(<WelcomeModal />, pluginSelf.mainWindow, {
			popupHeight: 475,
			popupWidth: 625,
		});
	});
