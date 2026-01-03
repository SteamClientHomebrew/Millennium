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

import { DialogButton, DialogControlsSection, IconsModule, pluginSelf, SteamSpinner } from '@steambrew/client';
import { MillenniumUpdateCard } from './MillenniumUpdateCard';
import { ThemeUpdateCard } from './ThemeUpdateCards';
import { useUpdateContext } from './useUpdateContext';
import { PluginUpdateCard } from './PluginUpdateCards';
import { locale } from '../../utils/localization-manager';
import { Placeholder } from '../../components/Placeholder';
import { settingsClasses } from '../../utils/classes';

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

const parseUpdateErrorMessage = () => {
	const themeError = pluginSelf?.updates?.themes?.error || '';
	const pluginError = pluginSelf?.updates?.plugins?.error || '';
	if (themeError && pluginError) return `${themeError}\n${pluginError}`;
	return themeError || pluginError;
};

const UpdatesViewModal: React.FC = () => {
	const { themeUpdates, pluginUpdates, hasUpdateError, hasReceivedUpdates, hasAnyUpdates, fetchAvailableUpdates } = useUpdateContext();

	if (hasUpdateError) {
		return (
			<Placeholder icon={<IconsModule.ExclamationPoint />} header={locale.updatePanelErrorHeader} body={locale.updatePanelErrorBody + parseUpdateErrorMessage()}>
				<DialogButton className={settingsClasses.SettingsDialogButton} onClick={fetchAvailableUpdates.spread(true)}>
					{locale.updatePanelErrorButton}
				</DialogButton>
			</Placeholder>
		);
	}

	if (!hasReceivedUpdates) return <SteamSpinner background="transparent" />;
	if (!hasAnyUpdates()) {
		return <Placeholder icon={<IconsModule.Checkmark />} header={locale.updatePanelNoUpdatesFoundHeader} body={locale.updatePanelNoUpdatesFound} />;
	}

	return (
		<RenderAvailableUpdates
			millenniumUpdates={pluginSelf.millenniumUpdates}
			themeUpdates={themeUpdates}
			pluginUpdates={pluginUpdates?.filter((update: any) => update?.hasUpdate)}
		/>
	);
};

export { UpdatesViewModal };
