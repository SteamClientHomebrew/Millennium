const MILLENNIUM_IS_CLIENT_MODULE = __IS_CLIENT__;
const pluginName = __PLUGIN_NAME__;
(window.PLUGIN_LIST ||= {})[pluginName] ||= {};
window.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS ||= {};
__FFI_STUBS__;
__FRONTEND_PROXY__;
var __millennium_plugin_console__ = (function () {
	__INSPECT_POLYFILL__;

	return function (level, file, line) {
		var args = Array.prototype.slice.call(arguments, 3);
		try {
			var MAX_MSG = 65536;
			var msg = args
				.map(function (a) {
					try {
						if (typeof a === 'string') return a;
						if (a === null) return 'null';
						if (a === undefined) return 'undefined';
						if (typeof a === 'number' || typeof a === 'boolean') return String(a);
						return __inspect(a, {
							depth: __DEPTH__,
							colors: __COLORS__,
							showHidden: __SHOW_HIDDEN__,
						});
					} catch (e) {
						return '[' + (e && e.message ? e.message : String(e)) + ']';
					}
				})
				.join(' ');
			if (msg.length > MAX_MSG) msg = msg.slice(0, MAX_MSG) + ' [truncated]';
			var payload = {
				plugin: __PLUGIN_NAME__,
				level: level,
				message: msg,
				source: '__SOURCE_TAG__',
			};
			if (file !== undefined) {
				payload.file = file;
				payload.line = line;
			}
			window.__millennium_plugin_console_binding__(JSON.stringify(payload));
		} catch (e) {
			try {
				var errPayload = {
					plugin: __PLUGIN_NAME__,
					level: 'error',
					message: '[console] ' + (e && e.message ? e.message : String(e)),
					source: '__SOURCE_TAG__',
				};
				window.__millennium_plugin_console_binding__(JSON.stringify(errPayload));
			} catch (_) {}
		}
	};
})();

let PluginEntryPointMain = function () {
	const __exports = {};
	const __bundle = __BUNDLE__;
	if (__bundle && typeof __bundle === 'object' && 'default' in __bundle) {
		Object.assign(__exports, __bundle);
	} else {
		__exports.default = __bundle;
	}
	return __exports;
};

(async () => {
	const PluginModule = PluginEntryPointMain();
	Object.assign(window.PLUGIN_LIST[pluginName], {
		...PluginModule,
		__millennium_internal_plugin_name_do_not_use_or_change__: pluginName,
	});
	const pluginProps = await PluginModule.default();
	if (pluginProps && pluginProps.title !== undefined && pluginProps.icon !== undefined && pluginProps.content !== undefined) {
		window.MILLENNIUM_SIDEBAR_NAVIGATION_PANELS[pluginName] = pluginProps;
	}
	__POST_MESSAGE__;
})();
