import { FC, SVGAttributes } from 'react';
import { Export, findModuleDetailsByExport, findModuleExport } from '../module-registry';

const isIconTable = (e: Export) => e && typeof e === 'object' && typeof e.Spinner === 'function' && typeof e.HardDrive === 'function' && Object.keys(e).length > 100;

const iconTable = findModuleExport(isIconTable);

const legacyIconsExport = iconTable
	? undefined
	: findModuleDetailsByExport((e) =>
			((s) => /Spinner\)}\)?,.\.createElement\("path",\{d:"M18 /.test(s) || /Spinner\)[\s\S]*\.jsxs?\)\("path",\{d:"M18 /.test(s))(e?.toString?.()),
		);

export const IconsModule = iconTable ?? legacyIconsExport?.[0];

/** @component React Components */
export const Spinner = (iconTable?.Spinner ?? legacyIconsExport?.[1]) as FC<SVGAttributes<SVGElement>>;
