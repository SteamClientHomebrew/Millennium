import { pluginSelf } from '@steambrew/client';
import { Utils } from '../../utils';
import { UpdateCard } from './UpdateCard';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';

export const MillenniumUpdateCard = ({ millenniumUpdates }: { millenniumUpdates: any }) => {
	if (!millenniumUpdates || !millenniumUpdates?.hasUpdate) return null;

	function VersionInformation() {
		return [
			'Millennium',
			<div className="MillenniumItem_Version">
				{pluginSelf.version} {'->'} {millenniumUpdates?.newVersion?.tag_name}
			</div>,
		];
	}

	return [
		<SettingsDialogSubHeader>Millennium</SettingsDialogSubHeader>,
		<UpdateCard
			update={{
				name: VersionInformation(),
				message: millenniumUpdates?.newVersion?.body,
				date: Utils.toTimeAgo(millenniumUpdates?.newVersion?.published_at),
				commit: millenniumUpdates?.newVersion?.html_url,
			}}
			index={0}
			totalCount={1}
			isUpdating={false}
			progress={0}
			statusText={String()}
			onUpdateClick={() => {}}
		/>,
	];
};
