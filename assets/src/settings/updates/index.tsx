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

import { DialogControlsSection, pluginSelf } from '@steambrew/client';
import { MillenniumUpdateCard } from './MillenniumUpdateCard';
import { ThemeUpdateCard } from './ThemeUpdateCards';
import { useUpdateContext } from './useUpdateContext';
import { PluginUpdateCard } from './PluginUpdateCards';

// TODO: Type this properly, this is a mess. Im too lazy to do it right now
interface UpdateProps {
	themeUpdates: any[];
	pluginUpdates: any[];
	millenniumUpdates: any;
}

const RenderAvailableUpdates: React.FC<UpdateProps> = ({ millenniumUpdates, themeUpdates, pluginUpdates }) => {
	return (
		<DialogControlsSection>
			{MillenniumUpdateCard({ millenniumUpdates })}
			{ThemeUpdateCard({ themeUpdates })}
			{PluginUpdateCard({ pluginUpdates })}
		</DialogControlsSection>
	);
};

const UpdatesViewModal: React.FC = () => {
	const { themeUpdates, pluginUpdates } = useUpdateContext();

	return (
		<RenderAvailableUpdates
			millenniumUpdates={pluginSelf.millenniumUpdates}
			themeUpdates={themeUpdates}
			pluginUpdates={pluginUpdates?.filter((update: any) => update?.hasUpdate)}
		/>
	);
};

export { UpdatesViewModal };
