const styles = {
	module: 'background: white; color: black; padding: 2px 6px; font-weight: bold;',
	name: 'background: rgb(181, 181, 181); color: black; padding: 2px 6px; font-weight: bold;',
	reset: 'background: transparent;',
	debug: 'background: #1abc9c; color: white; padding: 2px 6px; font-weight: bold;',
	warn: 'background: #ffbb00; color: white; padding: 2px 6px; font-weight: bold;',
	error: 'background: #FF0000; color: white; padding: 2px 6px; font-weight: bold;',
};

const format = (moduleName: string, name: string, style: string) => [`%c${moduleName}%c${name}%c`, styles.module, style, styles.reset];
// const log = (moduleName: string, name: string, ...args: any[]) => console.log(...format(moduleName, name, styles.name), ...args);
// const group = (moduleName: string, name: string, ...args: any[]) => console.group(...format(moduleName, name, styles.name), ...args);
// const debug = (moduleName: string, name: string, ...args: any[]) => console.debug(...format(moduleName, name, styles.debug), ...args);
// const warn = (moduleName: string, name: string, ...args: any[]) => console.warn(...format(moduleName, name, styles.warn), ...args);
// const error = (moduleName: string, name: string, ...args: any[]) => console.error(...format(moduleName, name, styles.error), ...args);

// const groupEnd = (...args: any[]) => {
// 	console.groupEnd();
// 	if (args.length) console.log(...args);
// };

export class Logger {
	log: (...args: any[]) => void;
	debug: (...args: any[]) => void;
	warn: (...args: any[]) => void;
	error: (...args: any[]) => void;
	// group: (...args: any[]) => void;

	constructor(name: string, moduleName = 'Millennium') {
		this.log = console.log.bind(console, ...format(moduleName, name, styles.name));
		this.debug = console.debug.bind(console, ...format(moduleName, name, styles.debug));
		this.warn = console.warn.bind(console, ...format(moduleName, name, styles.warn));
		this.error = console.error.bind(console, ...format(moduleName, name, styles.error));
		// this.group = console.group.bind(console, ...format(moduleName, name, styles.name));
	}

	// groupEnd(...args: any[]) {
	// 	groupEnd(...args);
	// }
}
