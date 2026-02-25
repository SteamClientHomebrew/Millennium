import { Logger } from './logger';

export const PrintParamHelp = () => {
	console.log(
		[
			'',
			'usage: millennium-ttc --build <dev|prod> [options]',
			'',
			'options:',
			'  --build <dev|prod>   build type (prod enables minification)',
			'  --target <path>      plugin directory (default: current directory)',
			'  --no-update          skip update check',
			'  --help               show this message',
			'',
		].join('\n'),
	);
};

export enum BuildType {
	DevBuild,
	ProdBuild,
}

export interface ParameterProps {
	type: BuildType;
	targetPlugin: string; // path
	isMillennium?: boolean;
}

export const ValidateParameters = (args: Array<string>): ParameterProps => {
	let typeProp: BuildType = BuildType.DevBuild,
		targetProp: string = process.cwd(),
		isMillennium: boolean = false;

	if (args.includes('--help')) {
		PrintParamHelp();
		process.exit();
	}

	if (!args.includes('--build')) {
		Logger.error('missing required argument: --build');
		PrintParamHelp();
		process.exit();
	}

	for (let i = 0; i < args.length; i++) {
		if (args[i] === '--build') {
			const BuildMode: string = args[i + 1];

			switch (BuildMode) {
				case 'dev':
					typeProp = BuildType.DevBuild;
					break;
				case 'prod':
					typeProp = BuildType.ProdBuild;
					break;
				default: {
					Logger.error('--build must be one of: dev, prod');
					process.exit();
				}
			}
		}

		if (args[i] == '--target') {
			if (args[i + 1] === undefined) {
				Logger.error('--target requires a path argument');
				process.exit();
			}

			targetProp = args[i + 1];
		}

		if (args[i] == '--millennium-internal') {
			isMillennium = true;
		}
	}

	return {
		type: typeProp,
		targetPlugin: targetProp,
		isMillennium: isMillennium,
	};
};
