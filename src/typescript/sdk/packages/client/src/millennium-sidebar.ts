/**
 * Drive the Millennium desktop sidebar (the quick-access panel that hosts plugin
 * settings) from a plugin - open its own panel, open another plugin's, or
 * open/close the list - instead of waiting for the user to click "Configure" in
 * the plugin manager.
 *
 * The implementation lives in Millennium's frontend, which owns the sidebar; it
 * injects itself via `registerMillenniumSidebar` when the sidebar mounts. Plugins
 * only ever call the public `MillenniumSidebar` methods.
 */
type MillenniumSidebarControls = {
	openPlugin: (pluginName: string) => Promise<boolean>;
	open: () => void;
	close: () => void;
};

let impl: MillenniumSidebarControls | undefined;

/**
 * Register the sidebar controls.
 *
 * @internal Called by Millennium's frontend when the sidebar mounts.
 */
export function registerMillenniumSidebar(controls: MillenniumSidebarControls): void {
	impl = controls;
}

export const MillenniumSidebar = {
	/**
	 * Open the sidebar focused on a plugin by its internal `name` (the same panel
	 * the plugin manager's "Configure" opens). Resolves `false` if there is no
	 * such plugin, or the sidebar is unavailable.
	 */
	openPlugin(pluginName: string): Promise<boolean> {
		return impl?.openPlugin(pluginName) ?? Promise.resolve(false);
	},

	/** Open the sidebar on its home (the plugin/theme list). */
	open(): void {
		impl?.open();
	},

	/** Close the sidebar. */
	close(): void {
		impl?.close();
	},
};
