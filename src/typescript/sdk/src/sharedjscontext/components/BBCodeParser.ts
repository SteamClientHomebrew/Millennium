import { FC } from 'react';

import { Export, findModuleExport, sourceOf } from '../module-registry';
import { nativeInternal } from '../internals';

export interface BBCodeParserProps {
	bShowShortSpeakerInfo?: boolean;
	event?: any;
	languageOverride?: any;
	showErrorInfo?: boolean;
	text?: string;
}

/** @component React Components */
export const BBCodeParser = (nativeInternal('BBCodeParser') ??
	findModuleExport((e: Export) => typeof e === 'function' && sourceOf(e).includes('ElementAccumulator') && sourceOf(e).includes('parser.ParseBBCode'))) as FC<BBCodeParserProps>;
