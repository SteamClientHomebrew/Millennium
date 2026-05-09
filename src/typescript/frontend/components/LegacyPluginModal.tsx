import { ConfirmModal, Field, Navigation, pluginSelf, showModal } from '@steambrew/client';
import { PluginComponent } from '../types';
import { locale } from '../utils/localization-manager';

const LegacyPluginModal = ({ plugins, closeModal }: { plugins: PluginComponent[]; closeModal: () => void }): React.ReactElement => {
	const viewUpdates = () => {
		closeModal();
		Navigation.Navigate('/millennium/settings/updates');
	};

	return (
		<ConfirmModal
			bDisableBackgroundDismiss={false}
			strTitle={locale.legacyPluginModalTitle}
			strDescription={
				<>
					<Field description={locale.legacyPluginModalBody} />
					{plugins.map((plugin, index) => (
						<Field
							key={plugin.data.name}
							label={
								<div className="MillenniumPlugins_PluginLabel">
									{plugin.data.common_name ?? plugin.data.name}
									{plugin.data.version && <div className="MillenniumItem_Version">{plugin.data.version}</div>}
								</div>
							}
							description={plugin.data.description}
							bottomSeparator={index === plugins.length - 1 ? 'none' : 'standard'}
						/>
					))}
				</>
			}
			bAlertDialog={false}
			bHideCloseIcon={false}
			strOKButtonText={locale.legacyPluginModalViewUpdates}
			strCancelButtonText={locale.legacyPluginModalDismiss}
			onOK={viewUpdates}
			onCancel={closeModal}
		/>
	);
};

export function showLegacyPluginModal(plugins: PluginComponent[]) {
	let modalWindow: ReturnType<typeof showModal>;
	const closeModal = () => modalWindow?.Close();

	modalWindow = showModal(<LegacyPluginModal plugins={plugins} closeModal={closeModal} />, pluginSelf.mainWindow, {
		popupHeight: 180 + plugins.length * 72,
		popupWidth: 580,
	});
}
