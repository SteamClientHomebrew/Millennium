/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { DialogButton, ErrorBoundary, IconsModule, PanelSection, PanelSectionRow } from '@steambrew/client';
import { PluginComponent } from '../types';
import { useDesktopMenu } from './DesktopMenuContext';
import { getPluginRenderers, getPluginView } from '../utils/globals';
import { Placeholder } from '../components/Placeholder';

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
	const pluginRenderers = getPluginRenderers();

	if (!pluginRenderers || Object.keys(pluginRenderers).length === 0) {
		return (
			<Placeholder
				icon={<IconsModule.TextCode />}
				header="No configurable plugins"
				body="Configurable plugins will appear here. No loaded plugins are currently configurable."
			/>
		);
	}

	return (
		<PanelSection>
			{Object.entries(pluginRenderers)?.map(([key, panel]: [string, any]) => (
				<RenderPluginViews plugins={plugins} key={key} pluginName={key} pluginView={panel} />
			))}
		</PanelSection>
	);
};
