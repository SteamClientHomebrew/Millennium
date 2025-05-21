import { Component } from 'react';
import { DialogControlsSection, pluginSelf } from '@steambrew/client';
import { locale } from '../../../locales';
import { MillenniumUpdateCard } from './MillenniumUpdateCard';
import { ThemeUpdateCard } from './ThemeUpdateCards';
import { RegisterUpdateListener, useUpdateContext } from './useUpdateContext';
import { PluginUpdateCard } from './PluginUpdateCards';
import { UpdateCountStyle } from '../../utils/styles';

// TODO: Type this properly, this is a mess. Im too lazy to do it right now
interface UpdateProps {
	themeUpdates: any[];
	pluginUpdates: any[];
	millenniumUpdates: any;
}

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ millenniumUpdates, themeUpdates, pluginUpdates }) => {
	return (
		<DialogControlsSection>
			<MillenniumUpdateCard millenniumUpdates={millenniumUpdates} />
			<ThemeUpdateCard themeUpdates={themeUpdates} />
			<PluginUpdateCard pluginUpdates={pluginUpdates} />
		</DialogControlsSection>
	);
};

const UpdatesViewModal: React.FC = () => {
	const { themeUpdates, pluginUpdates } = useUpdateContext();

	return (
		<RenderAvailableUpdates
			millenniumUpdates={pluginSelf.cachedMillenniumUpdateProps}
			themeUpdates={themeUpdates}
			pluginUpdates={pluginUpdates?.filter((update: any) => update?.hasUpdate)}
		/>
	);
};

class RenderUpdatesSettingsTab extends Component {
	state = {
		updateCount: 0,
	};

	GetUpdateCount = () => {
		const themeUpdates: any[] = pluginSelf.updates.themes;
		const pluginUpdates: any[] = pluginSelf.updates.plugins;

		return themeUpdates?.length + pluginUpdates?.filter?.((update: any) => update?.hasUpdate)?.length || 0;
	};

	componentDidMount() {
		this.unregister = RegisterUpdateListener(() => {
			this.setState({ updateCount: this.GetUpdateCount() });
		});

		this.setState({ updateCount: this.GetUpdateCount() });
	}

	componentWillUnmount() {
		if (this.unregister) {
			this.unregister();
		}
	}

	unregister?: () => void;

	render() {
		/** If there are errors don't render anything */
		if (Boolean(pluginSelf?.updates?.themes?.error || pluginSelf?.updates?.plugins?.error)) {
			return <>{locale.settingsPanelUpdates}</>;
		}

		return (
			<div className="sideBarUpdatesItem unreadFriend">
				<UpdateCountStyle />
				{locale.settingsPanelUpdates}
				{this.state.updateCount > 0 && (
					<div className="FriendMessageCount MillenniumUpdateCount" style={{ display: 'none' }}>
						{this.state.updateCount}
					</div>
				)}
			</div>
		);
	}
}

export { UpdatesViewModal, RenderUpdatesSettingsTab };
