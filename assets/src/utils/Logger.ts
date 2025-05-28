const Logger = {
	Error: (...message: any) => {
		console.error('%c Millennium ', 'background: red; color: white', ...message);
	},
	Log: (...message: any) => {
		console.log('%c Millennium ', 'background: purple; color: white', ...message);
	},
	Warn: (...message: any) => {
		console.warn('%c Millennium ', 'background: orange; color: white', ...message);
	},
	Debug: (...message: any) => {
		console.debug('%c Millennium ', 'background: blue; color: white', ...message);
	},
};

export { Logger };
