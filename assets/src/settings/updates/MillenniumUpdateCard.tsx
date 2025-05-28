import { pluginSelf } from '@steambrew/client';
import { Utils } from '../../utils';
import { UpdateCard } from './UpdateCard';
import { SettingsDialogSubHeader } from '../../components/SteamComponents';

export const MillenniumUpdateCard = ({ millenniumUpdates }: { millenniumUpdates: any }) => {
	if (!millenniumUpdates) return null;

	function VersionInformation() {
		return [
			'Millennium',
			<div className="MillenniumItem_Version">
				{pluginSelf.version} {'->'} {millenniumUpdates?.updates?.newVersion?.tag_name}
			</div>,
		];
	}

	return [
		<SettingsDialogSubHeader style={{ marginTop: '20px' }}>Millennium</SettingsDialogSubHeader>,
		<UpdateCard
			update={{
				name: <VersionInformation />,
				message: millenniumUpdates?.updates?.newVersion?.body,
				date: Utils.toTimeAgo(millenniumUpdates?.updates?.newVersion?.published_at),
				commit: millenniumUpdates?.updates?.newVersion?.html_url,
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
