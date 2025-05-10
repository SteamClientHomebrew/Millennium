const bgStyle1 = 'background: #8a16a2; color: white;';
const isSharedJSContext = window.location.hostname === 'steamloopback.host';

const moduleName = isSharedJSContext ? '@steambrew/client' : '@steambrew/webkit';

export const log = (name: string, ...args: any[]) => {
	console.log(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #b11cce; color: white;', 'background: transparent;', ...args);
};

export const group = (name: string, ...args: any[]) => {
	console.group(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #b11cce; color: white;', 'background: transparent;', ...args);
};

export const groupEnd = (name: string, ...args: any[]) => {
	console.groupEnd();
	if (args?.length > 0) console.log(`^ %c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #b11cce; color: white;', 'background: transparent;', ...args);
};

export const debug = (name: string, ...args: any[]) => {
	console.debug(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #1abc9c; color: white;', 'color: blue;', ...args);
};

export const warn = (name: string, ...args: any[]) => {
	console.warn(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #ffbb00; color: white;', 'color: blue;', ...args);
};

export const error = (name: string, ...args: any[]) => {
	console.error(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #FF0000;', 'background: transparent;', ...args);
};

class Logger {
	constructor(private name: string) {
		this.name = name;
	}

	log(...args: any[]) {
		log(this.name, ...args);
	}

	debug(...args: any[]) {
		debug(this.name, ...args);
	}

	warn(...args: any[]) {
		warn(this.name, ...args);
	}

	error(...args: any[]) {
		error(this.name, ...args);
	}

	group(...args: any[]) {
		group(this.name, ...args);
	}

	groupEnd(...args: any[]) {
		groupEnd(this.name, ...args);
	}
}

export default Logger;
