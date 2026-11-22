import { FC, ReactNode } from 'react';

import { Export, findModuleByExport, findModuleExport, sourceOf } from '../module-registry';

const ScrollingModule = findModuleByExport((e: Export) => sourceOf(e?.render).includes('{case"x":'));

const ScrollingModuleProps = ScrollingModule ? Object.values(ScrollingModule) : [];

/** @component React Components */
export const ScrollPanel = ScrollingModuleProps.find((prop: any) => sourceOf(prop?.render).includes('{case"x":')) as FC<{ children?: ReactNode }>;

/** @component React Components */
export const ScrollPanelGroup: FC<{ children?: ReactNode }> = findModuleExport((e: Export) => sourceOf(e?.render).includes('.FocusVisibleChild') && sourceOf(e?.render).includes('navRefPanel'));
