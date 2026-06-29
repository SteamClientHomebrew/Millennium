// adapted from node.js util polyfill (browserify), MIT licensed.
// https://github.com/browserify/node-util

function __hasCustomInspect(value) {
	return (
		!!value &&
		typeof value.inspect === 'function' &&
		value.inspect !== __inspect &&
		!(value.constructor && value.constructor.prototype === value)
	);
}

function __inspect(obj, opts) {
	if (opts === undefined) opts = {};
	var useColors = opts.colors !== undefined ? opts.colors : true;
	var ctx = {
		seen: [],
		stylize: useColors ? __stylizeWithColor : __stylizeNoColor,
		showHidden: opts.showHidden !== undefined ? opts.showHidden : false,
		depth: opts.depth === undefined ? 2 : opts.depth,
		colors: useColors,
		customInspect: opts.customInspect !== undefined ? opts.customInspect : true,
	};
	return __formatValue(ctx, obj, ctx.depth);
}

var __inspectStyles = {
	special: 'cyan', number: 'yellow', boolean: 'yellow', undefined: 'grey',
	null: 'bold', string: 'green', date: 'magenta', regexp: 'red',
};

var __ansiColors = {
	bold: [1, 22], italic: [3, 23], underline: [4, 24],
	inverse: [7, 27], white: [37, 39], grey: [90, 39],
	black: [30, 39], blue: [34, 39], cyan: [36, 39],
	green: [32, 39], magenta: [35, 39], red: [31, 39],
	yellow: [33, 39],
};
function __stylizeNoColor(str) { return str; }
function __stylizeWithColor(str, styleType) {
	var style = __ansiColors[__inspectStyles[styleType]];
	if (!style) return str;
	return '[' + style[0] + 'm' + str + '[' + style[1] + 'm';
}

function __arrayToHash(array) {
	var hash = {};
	for (var i = 0; i < array.length; i++) hash[array[i]] = true;
	return hash;
}

function __formatValue(ctx, value, recurseTimes) {
	if (ctx.customInspect && __hasCustomInspect(value)) {
		var ret = value.inspect(recurseTimes, ctx);
		if (typeof ret !== 'string') ret = __formatValue(ctx, ret, recurseTimes);
		return ret;
	}

	var primitive = __formatPrimitive(ctx, value);
	if (primitive !== undefined) return primitive;

	if (__isFunction(value)) {
		var fname = value.name ? ': ' + value.name : '';
		return ctx.stylize('[Function' + fname + ']', 'special');
	}

	var keys = Object.keys(value);
	var visibleKeys = __arrayToHash(keys);

	if (ctx.showHidden) keys = Object.getOwnPropertyNames(value);

	if (__isError(value) && (keys.indexOf('message') >= 0 || keys.indexOf('description') >= 0))
		return __formatError(value);

	if (keys.length === 0) {
		if (__isRegExp(value)) return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
		if (__isDate(value)) return ctx.stylize(Date.prototype.toString.call(value), 'date');
		if (__isError(value)) return __formatError(value);
	}

	var base = '', array = false, braces = ['{', '}'];

	if (Array.isArray(value)) { array = true; braces = ['[', ']']; }

	if (__isRegExp(value)) base = ' ' + RegExp.prototype.toString.call(value);
	if (__isDate(value)) base = ' ' + Date.prototype.toUTCString.call(value);
	if (__isError(value)) base = ' ' + __formatError(value);

	if (keys.length === 0 && (!array || value.length === 0))
		return braces[0] + base + braces[1];

	if (recurseTimes !== null && recurseTimes < 0) {
		if (__isRegExp(value)) return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
		return ctx.stylize('[Object]', 'special');
	}

	ctx.seen.push(value);

	var output;
	if (array) {
		output = __formatArray(ctx, value, recurseTimes, visibleKeys, keys);
	} else {
		output = keys.map(function (key) {
			try {
				return __formatProperty(ctx, value, recurseTimes, visibleKeys, key, false);
			} catch (e) {
				return key + ': ' + ctx.stylize('[Throws]', 'special');
			}
		});
	}

	ctx.seen.pop();
	return __reduceToSingleString(output, base, braces);
}

function __formatPrimitive(ctx, value) {
	if (value === undefined) return ctx.stylize('undefined', 'undefined');
	if (typeof value === 'string') {
		var simple = "'" +
			JSON.stringify(value).replace(/^"|"$/g, '').replace(/'/g, "\\'").replace(/\\"/g, '"') +
			"'";
		return ctx.stylize(simple, 'string');
	}
	if (typeof value === 'number') return ctx.stylize('' + value, 'number');
	if (typeof value === 'boolean') return ctx.stylize('' + value, 'boolean');
	if (value === null) return ctx.stylize('null', 'null');
	return undefined;
}

function __formatError(value) {
	return '[' + Error.prototype.toString.call(value) + ']';
}

function __formatArray(ctx, value, recurseTimes, visibleKeys, keys) {
	var output = [];
	for (var i = 0, l = value.length; i < l; ++i) {
		if (Object.prototype.hasOwnProperty.call(value, String(i))) {
			output.push(__formatProperty(ctx, value, recurseTimes, visibleKeys, String(i), true));
		} else {
			output.push('');
		}
	}
	for (var j = 0; j < keys.length; j++) {
		if (!/^\d+$/.test(keys[j]))
			output.push(__formatProperty(ctx, value, recurseTimes, visibleKeys, keys[j], true));
	}
	return output;
}

function __formatProperty(ctx, value, recurseTimes, visibleKeys, key, array) {
	var name, str;
	var desc;
	try {
		desc = Object.getOwnPropertyDescriptor(value, key) || { value: value[key] };
	} catch (e) {
		desc = null;
	}

	if (!desc) {
		str = ctx.stylize('[Throws]', 'special');
	} else if (desc.get) {
		str = desc.set ? ctx.stylize('[Getter/Setter]', 'special') : ctx.stylize('[Getter]', 'special');
	} else if (desc.set) {
		str = ctx.stylize('[Setter]', 'special');
	}

	if (!Object.prototype.hasOwnProperty.call(visibleKeys, key)) name = '[' + key + ']';

	if (!str) {
		if (ctx.seen.indexOf(desc.value) < 0) {
			str = recurseTimes === null
				? __formatValue(ctx, desc.value, null)
				: __formatValue(ctx, desc.value, recurseTimes - 1);
			if (str.indexOf('\n') > -1) {
				if (array) {
					str = str.split('\n').map(function (l) { return '  ' + l; }).join('\n').slice(2);
				} else {
					str = '\n' + str.split('\n').map(function (l) { return '   ' + l; }).join('\n');
				}
			}
		} else {
			str = ctx.stylize('[Circular]', 'special');
		}
	}

	if (name === undefined) {
		if (array && /^\d+$/.test(key)) return str;
		name = JSON.stringify('' + key);
		if (/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/.test(name)) {
			name = name.slice(1, -1);
			name = ctx.stylize(name, 'name');
		} else {
			name = name.replace(/'/g, "\\'").replace(/\\"/g, '"').replace(/(^"|"$)/g, "'");
			name = ctx.stylize(name, 'string');
		}
	}

	return name + ': ' + str;
}

function __reduceToSingleString(output, base, braces) {
	var length = output.reduce(function (prev, cur) {
		return prev + cur.replace(/\[\d\d?m/g, '').length + 1;
	}, 0);
	if (length > 60)
		return braces[0] + (base === '' ? '' : base + '\n ') + ' ' + output.join(',\n  ') + ' ' + braces[1];
	return braces[0] + base + ' ' + output.join(', ') + ' ' + braces[1];
}

function __isRegExp(re) { return typeof re === 'object' && re !== null && Object.prototype.toString.call(re) === '[object RegExp]'; }
function __isDate(d) { return typeof d === 'object' && d !== null && Object.prototype.toString.call(d) === '[object Date]'; }
function __isError(e) { return typeof e === 'object' && e !== null && (Object.prototype.toString.call(e) === '[object Error]' || e instanceof Error); }
function __isFunction(arg) { return typeof arg === 'function'; }
