import { DialogButton, ErrorBoundary, PanelSection, PanelSectionRow } from '@steambrew/client';
import { PluginComponent } from '../types';
import { useDesktopMenu } from './DesktopMenuContext';
import { getPluginRenderers, getPluginView } from '../utils/globals';

export const RenderPluginViews = ({ plugins, pluginName, pluginView }: { plugins: PluginComponent[]; pluginName: string; pluginView: any }) => {
	const { setActivePlugin } = useDesktopMenu();
	const plugin = plugins?.find((p) => p.data.name === pluginName);

	return (
		<PanelSectionRow key={pluginName}>
			<DialogButton style={{ padding: '0px 10px 0px 10px', width: '-webkit-fill-available' }} onClick={setActivePlugin.spread(plugin)}>
				<div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
					<div className="iconContainer">{pluginView?.icon}</div>
					<div>{plugin?.data?.common_name}</div>
				</div>
			</DialogButton>
		</PanelSectionRow>
	);
};

export const RenderPluginView = () => {
	const { activePlugin } = useDesktopMenu();
	const renderer = getPluginView(activePlugin?.data?.name)?.content;

	if (!renderer) {
		return (
			<PanelSection>
				<PanelSectionRow>
					Failed to find a renderer for <b>{activePlugin?.data?.name}</b>. Please check if the plugin is loaded correctly.
				</PanelSectionRow>
			</PanelSection>
		);
	}

	return <ErrorBoundary>{renderer}</ErrorBoundary>;
};

export const PluginSelectorView = () => {
	const { plugins } = useDesktopMenu();

	console.log('PluginSelectorView', plugins);

	if (!plugins || plugins.length === 0 || (plugins.length === 1 && plugins[0].data.name === 'core')) {
		return (
			<div className="NoPluginsFoundSection">
				<div className="NoPluginsFoundMessage">Configurable plugins will appear here. No loaded plugins are currently configurable.</div>
			</div>
		);
	}

	return (
		<PanelSection>
			{Object.entries(getPluginRenderers())?.map(([key, panel]: [string, any]) => (
				<RenderPluginViews plugins={plugins} key={key} pluginName={key} pluginView={panel} />
			))}
		</PanelSection>
	);
};
