import { FC, PropsWithChildren } from 'react';
import { findModuleExport } from '../webpack';

/** @component React Components */
export const ErrorBoundary = findModuleExport((e) => e.InstallErrorReportingStore && e?.prototype?.Reset && e?.prototype?.componentDidCatch) as FC<PropsWithChildren>; // Actually a class but @types/react is broken lol
