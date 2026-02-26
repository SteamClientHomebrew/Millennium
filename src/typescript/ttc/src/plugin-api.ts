/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

declare global {
	interface Window {
		/**
		 * @description The plugin list is a global object that contains all the plugins loaded in the client.
		 * It is used to store the pluginSelf object
		 */
		PLUGIN_LIST: any;
		/**
		 * @description Used to store the settings for the plugin.
		 * Millennium then renders these in the settings panel.
		 */
		MILLENNIUM_SIDEBAR_NAVIGATION_PANELS: any;
	}
}

/**
 * the pluginName is the name of the plugin.
 * It is used to identify the plugin in the plugin list and settings store.
 */
declare const pluginName: string;
/**
 * PluginEntryPointMain is the default function returned by rolling up the plugin.
 * It is the main entry point for the plugin. It's IIFE has been removed, therefore it only runs once its manually called.
 * This is done to prevent the plugin from running before the settings have been parsed.
 */
declare const PluginEntryPointMain: any;
/**
 * The underlying IPC object used to communicate with the backend.
 * Its defined within the plugin utils package under the client module.
 */
declare const MILLENNIUM_BACKEND_IPC: any;
/**
 * Since ExecutePluginModule is called from both the webkit and the client module,
 * this flag is used to determine if the plugin is running in the client module or not.
 */
declare const MILLENNIUM_IS_CLIENT_MODULE: boolean;

/**
 * @description Append the active plugin to the global plugin
 * list and notify that the frontend Loaded.
 */
async function ExecutePluginModule() {
	let PluginModule = PluginEntryPointMain();

	/** Assign the plugin on plugin list. */
	Object.assign(window.PLUGIN_LIST[pluginName], {
		...PluginModule,
		__millennium_internal_plugin_name_do_not_use_or_change__: pluginName,
	});

	/** Run the rolled up plugins default exported function */
	let pluginProps = await PluginModule.default();

	function isValidSidebarNavComponent(obj: any) {
		return obj && obj.title !== undefined && obj.icon !== undefined && obj.content !== undefined;
	}

	if (pluginProps && isValidSidebarNavComponent(pluginProps)) {
		window.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS[pluginName] = pluginProps;
	}

	/** If the current module is a client module, post message id=1 which calls the front_end_loaded method on the backend. */
	if (MILLENNIUM_IS_CLIENT_MODULE) {
		MILLENNIUM_BACKEND_IPC.postMessage(1, { pluginName: pluginName });
	}
}

/**
 * @description Initialize the plugins settings store and the plugin list.
 * This function is called once per plugin and is used to store the plugin settings and the plugin list.
 */
function InitializePlugins() {
	(window.PLUGIN_LIST ||= {})[pluginName] ||= {};
	window.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS ||= {};
}

export { ExecutePluginModule, InitializePlugins };
