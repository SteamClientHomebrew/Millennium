import { ConfirmModal, Field, pluginSelf, showModal } from '@steambrew/sdk';
import { PluginComponent } from '../types';
import { locale } from '../utils/localization-manager';
import { backend } from '../utils/ffi';
import { Installer, InstallType } from '../settings/general/Installer';
import { Logger } from '../utils/Logger';
import Markdown from 'markdown-to-jsx';

const EXTENDIUM_ID = '788ed8554492';

export const SUPERSEDED_PLUGIN_NAMES = ['augmented-steam', 'steam-db'];

const SupersededPluginModal = ({ plugins, closeModal, onDismiss }: { plugins: PluginComponent[]; closeModal: () => void; onDismiss: () => void }): React.ReactElement => {
	const installExtendium = async () => {
		closeModal();

		for (const plugin of plugins) {
			try {
				await backend.plugins.uninstall(plugin.data.name);
			} catch (e) {
				Logger.Error(`Failed to uninstall superseded plugin ${plugin.data.name}`, e);
			}
		}

		const installer = new Installer();
		installer.OpenInstallPrompt(EXTENDIUM_ID, InstallType.Plugin, () => {});
	};

	return (
		<ConfirmModal
			bDisableBackgroundDismiss={true}
			strTitle={locale.supersededPluginModalTitle}
			strDescription={
				<>
					<Field description={<Markdown options={{ overrides: { a: { props: { target: '_blank' } } } }}>{locale.supersededPluginModalBody}</Markdown>} />
					{plugins.map((plugin) => (
						<Field
							key={plugin.data.name}
							label={
								<div className="MillenniumPlugins_PluginLabel">
									{plugin.data.common_name ?? plugin.data.name}
									{plugin.data.version && <div className="MillenniumItem_Version">{plugin.data.version}</div>}
								</div>
							}
							description={plugin.data.description}
						/>
					))}
				</>
			}
			bAlertDialog={false}
			bHideCloseIcon={true}
			strOKButtonText={locale.supersededPluginModalInstall}
			strCancelButtonText={locale.supersededPluginModalDismiss}
			onOK={installExtendium}
			onCancel={() => {
				closeModal();
				onDismiss();
			}}
		/>
	);
};

export function showSupersededPluginModal(plugins: PluginComponent[], onDismiss?: () => void) {
	let modalWindow: ReturnType<typeof showModal>;
	const closeModal = () => modalWindow?.Close();

	modalWindow = showModal(<SupersededPluginModal plugins={plugins} closeModal={closeModal} onDismiss={onDismiss ?? (() => {})} />, pluginSelf.mainWindow, {
		popupHeight: 180 + plugins.length * 72,
		popupWidth: 580,
	});
}
