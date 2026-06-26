import { FC, SVGAttributes } from 'react';
import { findModuleDetailsByExport } from '../webpack';

const iconsModuleExport = findModuleDetailsByExport((e) =>
	((s) => /Spinner\)}\)?,.\.createElement\("path",\{d:"M18 /.test(s) || /Spinner\)[\s\S]*\.jsxs?\)\("path",\{d:"M18 /.test(s))(e?.toString?.()),
);

export const IconsModule = iconsModuleExport[0];

/** @component React Components */
export const Spinner = iconsModuleExport[1] as FC<SVGAttributes<SVGElement>>;
