import { CSSProperties, FC } from 'react';

import { Export, findModuleExport, sourceOf } from '../module-registry';
import { nativeInternal } from '../internals';

export interface MarqueeProps {
	play?: boolean;
	direction?: 'left' | 'right';
	speed?: number;
	delay?: number;
	fadeLength?: number;
	center?: boolean;
	resetOnPause?: boolean;
	style?: CSSProperties;
	className?: string;
	children: React.ReactNode;
}

/** @component React Components */
export const Marquee: FC<MarqueeProps> = nativeInternal('Marquee') ?? findModuleExport((e: Export) => sourceOf(e).includes('.Marquee') && sourceOf(e).includes('--fade-length'));
