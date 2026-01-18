const styles = {
	module: 'background: white; color: black; padding: 2px 6px; font-weight: bold;',
	name: 'background: rgb(181, 181, 181); color: black; padding: 2px 6px; font-weight: bold;',
	reset: 'background: transparent;',
	debug: 'background: #1abc9c; color: white; padding: 2px 6px; font-weight: bold;',
	warn: 'background: #ffbb00; color: white; padding: 2px 6px; font-weight: bold;',
	error: 'background: #FF0000; color: white; padding: 2px 6px; font-weight: bold;',
};

const format = (moduleName: string, name: string, style: string) => [`%c${moduleName}%c${name}%c`, styles.module, style, styles.reset];
export const log = (moduleName: string, name: string, ...args: any[]) => console.log(...format(moduleName, name, styles.name), ...args);
export const group = (moduleName: string, name: string, ...args: any[]) => console.group(...format(moduleName, name, styles.name), ...args);
export const debug = (moduleName: string, name: string, ...args: any[]) => console.debug(...format(moduleName, name, styles.debug), ...args);
export const warn = (moduleName: string, name: string, ...args: any[]) => console.warn(...format(moduleName, name, styles.warn), ...args);
export const error = (moduleName: string, name: string, ...args: any[]) => console.error(...format(moduleName, name, styles.error), ...args);

export const groupEnd = (...args: any[]) => {
	console.groupEnd();
	if (args.length) console.log(...args);
};

export default class Logger {
	constructor(private name: string, private moduleName = 'Millennium') {}
	log(...args: any[]) {
		log(this.moduleName, this.name, ...args);
	}
	debug(...args: any[]) {
		debug(this.moduleName, this.name, ...args);
	}
	warn(...args: any[]) {
		warn(this.moduleName, this.name, ...args);
	}
	error(...args: any[]) {
		error(this.moduleName, this.name, ...args);
	}
	group(...args: any[]) {
		group(this.moduleName, this.name, ...args);
	}
	groupEnd(...args: any[]) {
		groupEnd(...args);
	}
}
