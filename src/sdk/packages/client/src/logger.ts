const bgStyle1 = 'background:rgb(37, 105, 184); color: white;';

export const log = (moduleName: string, name: string, ...args: any[]) => {
	console.log(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background:rgb(28, 135, 206); color: white;', 'background: transparent;', ...args);
};

export const group = (moduleName: string, name: string, ...args: any[]) => {
	console.group(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: rgb(28, 135, 206); color: white;', 'background: transparent;', ...args);
};

export const groupEnd = (moduleName: string, name: string, ...args: any[]) => {
	console.groupEnd();
	if (args?.length > 0) console.log(`^ %c ${moduleName} %c ${name} %c`, bgStyle1, 'background: rgb(28, 135, 206); color: white;', 'background: transparent;', ...args);
};

export const debug = (moduleName: string, name: string, ...args: any[]) => {
	console.debug(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #1abc9c; color: white;', 'color: blue;', ...args);
};

export const warn = (moduleName: string, name: string, ...args: any[]) => {
	console.warn(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #ffbb00; color: white;', 'color: blue;', ...args);
};

export const error = (moduleName: string, name: string, ...args: any[]) => {
	console.error(`%c ${moduleName} %c ${name} %c`, bgStyle1, 'background: #FF0000;', 'background: transparent;', ...args);
};

class Logger {
	constructor(private name: string, private moduleName: string = 'Millennium') {
		this.name = name;
		this.moduleName = moduleName;
	}

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
		groupEnd(this.moduleName, this.name, ...args);
	}
}

export default Logger;
