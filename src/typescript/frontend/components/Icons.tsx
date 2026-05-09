/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { motion, useAnimation } from 'motion/react';
import type { Transition, Variants } from 'motion/react';
import { forwardRef, useCallback, useEffect, useImperativeHandle, useRef, type HTMLAttributes } from 'react';
import { pagedSettingsListItemClass } from '../utils/classes';

function useNavItemAnimation(
	divRef: React.RefObject<HTMLDivElement | null>,
	controlled: React.MutableRefObject<boolean>,
	controls: ReturnType<typeof useAnimation>,
	enterVariant: string,
	leaveVariant: string,
) {
	const inNavItem = useRef(false);
	useEffect(() => {
		if (controlled.current || !divRef.current || !pagedSettingsListItemClass) return;
		let el: HTMLElement | null = divRef.current.parentElement;
		while (el && !el.classList.contains(pagedSettingsListItemClass)) el = el.parentElement;
		if (!el) return;
		inNavItem.current = true;
		const navItem = el;
		const onEnter = () => controls.start(enterVariant);
		const onLeave = () => controls.start(leaveVariant);
		navItem.addEventListener('mouseenter', onEnter);
		navItem.addEventListener('mouseleave', onLeave);
		return () => {
			inNavItem.current = false;
			navItem.removeEventListener('mouseenter', onEnter);
			navItem.removeEventListener('mouseleave', onLeave);
		};
	// eslint-disable-next-line react-hooks/exhaustive-deps
	}, []);
	return inNavItem;
}

// ─── ConnectIcon (Plugins) ───────────────────────────────────────────────────

const CONNECT_PLUG_VARIANTS: Variants = {
	normal: { x: 0, y: 0 },
	animate: { x: -3, y: 3 },
};
const CONNECT_SOCKET_VARIANTS: Variants = {
	normal: { x: 0, y: 0 },
	animate: { x: 3, y: -3 },
};
const CONNECT_PATH_VARIANTS = {
	normal: (c: { x: number; y: number }) => ({ d: `M${c.x} ${c.y} l2.5 -2.5` }),
	animate: (c: { x: number; y: number }) => ({ d: `M${c.x + 2.93} ${c.y - 2.93} l0.10 -0.10` }),
};

const ConnectIcon = forwardRef<{ startAnimation: () => void; stopAnimation: () => void }, HTMLAttributes<HTMLDivElement> & { size?: number }>(
	({ onMouseEnter, onMouseLeave, className, size = 28, ...props }, ref) => {
		const controls = useAnimation();
		const controlled = useRef(false);
		const divRef = useRef<HTMLDivElement>(null);
		useImperativeHandle(ref, () => {
			controlled.current = true;
			return { startAnimation: () => controls.start('animate'), stopAnimation: () => controls.start('normal') };
		});
		const inNavItem = useNavItemAnimation(divRef, controlled, controls, 'animate', 'normal');
		const onEnter = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseEnter?.(e); else if (!inNavItem.current) controls.start('animate'); }, [controls, inNavItem, onMouseEnter]);
		const onLeave = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseLeave?.(e); else if (!inNavItem.current) controls.start('normal'); }, [controls, inNavItem, onMouseLeave]);
		return (
			<div ref={divRef} className={className} onMouseEnter={onEnter} onMouseLeave={onLeave} {...props}>
				<svg fill="none" height={size} width={size} stroke="currentColor" strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
					<motion.path animate={controls} d="M19 5l3 -3" transition={{ type: 'spring', stiffness: 500, damping: 30 }} variants={{ normal: { d: 'M19 5l3 -3' }, animate: { d: 'M17 7l5 -5' } }} />
					<motion.path animate={controls} d="m2 22 3-3" transition={{ type: 'spring', stiffness: 500, damping: 30 }} variants={{ normal: { d: 'm2 22 3-3' }, animate: { d: 'm2 22 6-6' } }} />
					<motion.path animate={controls} d="M6.3 20.3a2.4 2.4 0 0 0 3.4 0L12 18l-6-6-2.3 2.3a2.4 2.4 0 0 0 0 3.4Z" transition={{ type: 'spring', stiffness: 500, damping: 30 }} variants={CONNECT_SOCKET_VARIANTS} />
					<motion.path animate={controls} custom={{ x: 7.5, y: 13.5 }} initial="normal" transition={{ type: 'spring', stiffness: 500, damping: 30 }} variants={CONNECT_PATH_VARIANTS} />
					<motion.path animate={controls} custom={{ x: 10.5, y: 16.5 }} initial="normal" transition={{ type: 'spring', stiffness: 500, damping: 30 }} variants={CONNECT_PATH_VARIANTS} />
					<motion.path animate={controls} d="m12 6 6 6 2.3-2.3a2.4 2.4 0 0 0 0-3.4l-2.6-2.6a2.4 2.4 0 0 0-3.4 0Z" transition={{ type: 'spring', stiffness: 500, damping: 30 }} variants={CONNECT_PLUG_VARIANTS} />
				</svg>
			</div>
		);
	}
);
ConnectIcon.displayName = 'ConnectIcon';

// ─── CoffeeIcon (General) ────────────────────────────────────────────────────

const COFFEE_PATH_VARIANTS: Variants = {
	normal: { y: 0, opacity: 1 },
	animate: (custom: number) => ({
		y: -3,
		opacity: [0, 1, 0],
		transition: { repeat: Number.POSITIVE_INFINITY, duration: 1.5, ease: 'easeInOut', delay: 0.2 * custom },
	}),
};

const CoffeeIcon = forwardRef<{ startAnimation: () => void; stopAnimation: () => void }, HTMLAttributes<HTMLDivElement> & { size?: number }>(
	({ onMouseEnter, onMouseLeave, className, size = 28, ...props }, ref) => {
		const controls = useAnimation();
		const controlled = useRef(false);
		const divRef = useRef<HTMLDivElement>(null);
		useImperativeHandle(ref, () => {
			controlled.current = true;
			return { startAnimation: () => controls.start('animate'), stopAnimation: () => controls.start('normal') };
		});
		const inNavItem = useNavItemAnimation(divRef, controlled, controls, 'animate', 'normal');
		const onEnter = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseEnter?.(e); else if (!inNavItem.current) controls.start('animate'); }, [controls, inNavItem, onMouseEnter]);
		const onLeave = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseLeave?.(e); else if (!inNavItem.current) controls.start('normal'); }, [controls, inNavItem, onMouseLeave]);
		return (
			<div ref={divRef} className={className} onMouseEnter={onEnter} onMouseLeave={onLeave} {...props}>
				<svg fill="none" height={size} width={size} stroke="currentColor" strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" style={{ overflow: 'visible' }} viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
					<motion.path animate={controls} custom={0.2} d="M10 2v2" variants={COFFEE_PATH_VARIANTS} />
					<motion.path animate={controls} custom={0.4} d="M14 2v2" variants={COFFEE_PATH_VARIANTS} />
					<motion.path animate={controls} custom={0} d="M6 2v2" variants={COFFEE_PATH_VARIANTS} />
					<path d="M16 8a1 1 0 0 1 1 1v8a4 4 0 0 1-4 4H7a4 4 0 0 1-4-4V9a1 1 0 0 1 1-1h14a4 4 0 1 1 0 8h-1" />
				</svg>
			</div>
		);
	}
);
CoffeeIcon.displayName = 'CoffeeIcon';

// ─── ContrastIcon (Themes) ───────────────────────────────────────────────────

const CONTRAST_PATH_VARIANT: Variants = {
	normal: { rotate: 0 },
	animate: { rotate: 180, transformOrigin: 'left center', transition: { type: 'spring', stiffness: 80, damping: 12 } },
};

const ContrastIcon = forwardRef<{ startAnimation: () => void; stopAnimation: () => void }, HTMLAttributes<HTMLDivElement> & { size?: number }>(
	({ onMouseEnter, onMouseLeave, className, size = 28, ...props }, ref) => {
		const controls = useAnimation();
		const controlled = useRef(false);
		const divRef = useRef<HTMLDivElement>(null);
		useImperativeHandle(ref, () => {
			controlled.current = true;
			return { startAnimation: () => controls.start('animate'), stopAnimation: () => controls.start('normal') };
		});
		const inNavItem = useNavItemAnimation(divRef, controlled, controls, 'animate', 'normal');
		const onEnter = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseEnter?.(e); else if (!inNavItem.current) controls.start('animate'); }, [controls, inNavItem, onMouseEnter]);
		const onLeave = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseLeave?.(e); else if (!inNavItem.current) controls.start('normal'); }, [controls, inNavItem, onMouseLeave]);
		return (
			<div ref={divRef} className={className} onMouseEnter={onEnter} onMouseLeave={onLeave} {...props}>
				<svg fill="none" height={size} width={size} stroke="currentColor" strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
					<circle cx="12" cy="12" r="10" />
					<motion.path animate={controls} d="M12 18a6 6 0 0 0 0-12v12z" initial="normal" variants={CONTRAST_PATH_VARIANT} />
				</svg>
			</div>
		);
	}
);
ContrastIcon.displayName = 'ContrastIcon';

// ─── CloudDownloadIcon (Updates) ─────────────────────────────────────────────

const CLOUD_VARIANTS: Variants = {
	initial: { y: 2 },
	active: { y: 0 },
};

const CloudDownloadIcon = forwardRef<{ startAnimation: () => void; stopAnimation: () => void }, HTMLAttributes<HTMLDivElement> & { size?: number }>(
	({ onMouseEnter, onMouseLeave, className, size = 28, ...props }, ref) => {
		const controls = useAnimation();
		const controlled = useRef(false);
		const divRef = useRef<HTMLDivElement>(null);
		useImperativeHandle(ref, () => {
			controlled.current = true;
			return { startAnimation: () => controls.start('initial'), stopAnimation: () => controls.start('active') };
		});
		const inNavItem = useNavItemAnimation(divRef, controlled, controls, 'initial', 'active');
		const onEnter = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseEnter?.(e); else if (!inNavItem.current) controls.start('initial'); }, [controls, inNavItem, onMouseEnter]);
		const onLeave = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseLeave?.(e); else if (!inNavItem.current) controls.start('active'); }, [controls, inNavItem, onMouseLeave]);
		return (
			<div ref={divRef} className={className} onMouseEnter={onEnter} onMouseLeave={onLeave} {...props}>
				<svg fill="none" height={size} width={size} stroke="currentColor" strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
					<path d="M4.2 15.1A7 7 0 1 1 15.71 8h1.79a4.5 4.5 0 0 1 2.5 8.2" />
					<motion.g animate={controls} transition={{ duration: 0.3, ease: [0.68, -0.6, 0.32, 1.6] }} variants={CLOUD_VARIANTS}>
						<path d="M12 13v8l-4-4" />
						<path d="m12 21 4-4" />
					</motion.g>
				</svg>
			</div>
		);
	}
);
CloudDownloadIcon.displayName = 'CloudDownloadIcon';

// ─── CctvIcon (Logs) ─────────────────────────────────────────────────────────

const CCTV_GROUP_VARIANTS: Variants = {
	normal: { rotate: 0, y: 0, x: 0 },
	animate: { rotate: [0, -20, -20, 15, 15, 0], y: [0, -0.5, -0.5, 0, 0, 0], x: [0, 0, 0, 0.5, 0.5, 0], transition: { duration: 1.8, ease: 'easeInOut' } },
};
const CCTV_PATH_VARIANTS: Variants = {
	normal: { opacity: 1 },
	animate: { opacity: [1, 0, 1, 0, 1, 0, 1], transition: { duration: 1.8, ease: 'easeInOut' } },
};

const CctvIcon = forwardRef<{ startAnimation: () => void; stopAnimation: () => void }, HTMLAttributes<HTMLDivElement> & { size?: number }>(
	({ onMouseEnter, onMouseLeave, className, size = 28, ...props }, ref) => {
		const controls = useAnimation();
		const controlled = useRef(false);
		const divRef = useRef<HTMLDivElement>(null);
		useImperativeHandle(ref, () => {
			controlled.current = true;
			return { startAnimation: () => controls.start('animate'), stopAnimation: () => controls.start('normal') };
		});
		const inNavItem = useNavItemAnimation(divRef, controlled, controls, 'animate', 'normal');
		const onEnter = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseEnter?.(e); else if (!inNavItem.current) controls.start('animate'); }, [controls, inNavItem, onMouseEnter]);
		const onLeave = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseLeave?.(e); else if (!inNavItem.current) controls.start('normal'); }, [controls, inNavItem, onMouseLeave]);
		return (
			<div ref={divRef} className={className} onMouseEnter={onEnter} onMouseLeave={onLeave} {...props}>
				<svg fill="none" height={size} width={size} stroke="currentColor" strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
					<motion.g animate={controls} initial="normal" variants={CCTV_GROUP_VARIANTS}>
						<path d="M16.75 12h3.632a1 1 0 0 1 .894 1.447l-2.034 4.069a1 1 0 0 1-1.708.134l-2.124-2.97" />
						<path d="M17.106 9.053a1 1 0 0 1 .447 1.341l-3.106 6.211a1 1 0 0 1-1.342.447L3.61 12.3a2.92 2.92 0 0 1-1.3-3.91L3.69 5.6a2.92 2.92 0 0 1 3.92-1.3z" />
						<motion.path animate={controls} d="M7 9h.01" variants={CCTV_PATH_VARIANTS} />
					</motion.g>
					<path d="M2 19h3.76a2 2 0 0 0 1.8-1.1L9 15" />
					<path d="M2 21v-4" />
				</svg>
			</div>
		);
	}
);
CctvIcon.displayName = 'CctvIcon';

// ─── RabbitIcon (QuickCSS) ───────────────────────────────────────────────────

const RABBIT_TRANSITION: Transition = {
	duration: 0.6,
	ease: [0.42, 0, 0.58, 1],
};

const RABBIT_VARIANTS: Variants = {
	normal: { rotate: 0, x: 0, y: 0 },
	animate: {
		rotate: [0, 5, -5, 3, -3, 0],
		x: [0, 3, -3, 2, -2, 0],
		y: [0, 1.5, -1.5, 1, -1, 0],
		transition: RABBIT_TRANSITION,
	},
};

const RabbitIcon = forwardRef<{ startAnimation: () => void; stopAnimation: () => void }, HTMLAttributes<HTMLDivElement> & { size?: number }>(
	({ onMouseEnter, onMouseLeave, className, size = 28, ...props }, ref) => {
		const controls = useAnimation();
		const controlled = useRef(false);
		const divRef = useRef<HTMLDivElement>(null);
		useImperativeHandle(ref, () => {
			controlled.current = true;
			return { startAnimation: () => controls.start('animate'), stopAnimation: () => controls.start('normal') };
		});
		const inNavItem = useNavItemAnimation(divRef, controlled, controls, 'animate', 'normal');
		const onEnter = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseEnter?.(e); else if (!inNavItem.current) controls.start('animate'); }, [controls, inNavItem, onMouseEnter]);
		const onLeave = useCallback((e: React.MouseEvent<HTMLDivElement>) => { if (controlled.current) onMouseLeave?.(e); else if (!inNavItem.current) controls.start('normal'); }, [controls, inNavItem, onMouseLeave]);
		return (
			<div ref={divRef} className={className} onMouseEnter={onEnter} onMouseLeave={onLeave} {...props}>
				<motion.svg
					animate={controls}
					fill="none"
					height={size}
					stroke="currentColor"
					strokeLinecap="round"
					strokeLinejoin="round"
					strokeWidth="2"
					variants={RABBIT_VARIANTS}
					viewBox="0 0 24 24"
					width={size}
					xmlns="http://www.w3.org/2000/svg"
				>
					<path d="M18 21h-8a4 4 0 0 1-4-4 7 7 0 0 1 7-7h.2L9.6 6.4a1 1 0 1 1 2.8-2.8L15.8 7h.2c3.3 0 6 2.7 6 6v1a2 2 0 0 1-2 2h-1a3 3 0 0 0-3 3" />
					<path d="M13 16a3 3 0 0 1 2.24 5" />
					<path d="M18 12h.01" />
					<path d="M20 8.54V4a2 2 0 1 0-4 0v3" />
					<path d="M7.612 12.524a3 3 0 1 0-1.6 4.3" />
				</motion.svg>
			</div>
		);
	}
);
RabbitIcon.displayName = 'RabbitIcon';

const MillenniumIcons = {
	SteamBrewLogo: () => (
		<svg xmlns="http://www.w3.org/2000/svg" xmlnsXlink="http://www.w3.org/1999/xlink" viewBox="0,0,256,256" fill-rule="nonzero">
			<g transform="translate(-40.96,-40.96) scale(1.32,1.32)">
				<g
					fill="currentColor"
					fill-rule="nonzero"
					stroke="none"
					stroke-width="1"
					stroke-linecap="butt"
					stroke-linejoin="miter"
					stroke-miterlimit="10"
					stroke-dasharray=""
					stroke-dashoffset="0"
					font-family="none"
					font-weight="none"
					font-size="none"
					text-anchor="none"
					style={{ mixBlendMode: 'normal' }}
				>
					<g transform="scale(5.33333,5.33333)">
						<path d="M17.5,27c-0.9,0 -1.74,0.27 -2.45,0.73l3.43,1.47c1.27,0.55 1.86,2.02 1.32,3.28c-0.41,0.95 -1.33,1.52 -2.3,1.52c-0.33,0 -0.66,-0.06 -0.98,-0.2l-3.44,-1.47c0.39,2.08 2.22,3.67 4.42,3.67c2.48,0 4.5,-2.02 4.5,-4.5c0,-2.48 -2.02,-4.5 -4.5,-4.5zM30,13c-2.76,0 -5,2.24 -5,5c0,2.76 2.24,5 5,5c2.76,0 5,-2.24 5,-5c0,-2.76 -2.24,-5 -5,-5zM30,21c-1.66,0 -3,-1.34 -3,-3c0,-1.66 1.34,-3 3,-3c1.66,0 3,1.34 3,3c0,1.66 -1.34,3 -3,3zM36.5,6h-25c-3.03,0 -5.5,2.47 -5.5,5.5v12.35l6.98,2.99c1.17,-1.14 2.76,-1.84 4.52,-1.84c0.16,0 0.33,0.01 0.49,0.02l4.07,-6.1c-0.04,-0.3 -0.06,-0.61 -0.06,-0.92c0,-4.41 3.59,-8 8,-8c4.41,0 8,3.59 8,8c0,4.41 -3.59,8 -8,8c-0.14,0 -0.28,-0.01 -0.42,-0.02l-5.65,4.62c0.04,0.3 0.07,0.59 0.07,0.9c0,3.58 -2.92,6.5 -6.5,6.5c-3.58,0 -6.5,-2.92 -6.5,-6.5v-0.06l-5,-2.15v7.21c0,3.03 2.47,5.5 5.5,5.5h25c3.03,0 5.5,-2.47 5.5,-5.5v-25c0,-3.03 -2.47,-5.5 -5.5,-5.5z"></path>
					</g>
				</g>
			</g>
		</svg>
	),
	RabbitRunning: () => (
		<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
			<path d="M 409.77777777777777 63.111111111111114 L 408 61.333333333333336 L 409.77777777777777 63.111111111111114 L 408 61.333333333333336 Q 400.8888888888889 55.111111111111114 392 57.77777777777778 Q 383.1111111111111 59.55555555555556 380.44444444444446 69.33333333333333 L 379.55555555555554 71.11111111111111 L 379.55555555555554 71.11111111111111 Q 375.1111111111111 87.11111111111111 374.22222222222223 103.11111111111111 Q 407.1111111111111 131.55555555555554 422.22222222222223 173.33333333333334 Q 410.6666666666667 170.66666666666666 400 170.66666666666666 Q 399.1111111111111 170.66666666666666 398.22222222222223 170.66666666666666 Q 385.77777777777777 141.33333333333334 361.77777777777777 120 Q 337.77777777777777 99.55555555555556 305.77777777777777 91.55555555555556 L 303.1111111111111 90.66666666666667 L 303.1111111111111 90.66666666666667 Q 294.22222222222223 88 286.22222222222223 92.44444444444444 Q 271.1111111111111 103.11111111111111 276.44444444444446 120 Q 297.77777777777777 174.22222222222223 348.44444444444446 200 Q 344 208.88888888888889 342.22222222222223 219.55555555555554 L 248.88888888888889 166.22222222222223 L 248.88888888888889 166.22222222222223 Q 206.22222222222223 142.22222222222223 158.22222222222223 142.22222222222223 Q 112.88888888888889 144 88 181.33333333333334 Q 73.77777777777777 164.44444444444446 49.77777777777778 163.55555555555554 Q 28.444444444444443 164.44444444444446 14.222222222222221 177.77777777777777 Q 0.8888888888888888 192 0 213.33333333333334 Q 0.8888888888888888 234.66666666666666 14.222222222222221 248.88888888888889 Q 28.444444444444443 262.22222222222223 49.77777777777778 263.1111111111111 Q 64.88888888888889 263.1111111111111 77.33333333333333 255.11111111111111 Q 83.55555555555556 278.22222222222223 102.22222222222223 296 L 228.44444444444446 411.55555555555554 L 228.44444444444446 411.55555555555554 Q 245.33333333333334 426.6666666666667 266.6666666666667 426.6666666666667 L 369.77777777777777 426.6666666666667 L 369.77777777777777 426.6666666666667 Q 382.22222222222223 426.6666666666667 390.22222222222223 418.6666666666667 Q 398.22222222222223 410.6666666666667 398.22222222222223 398.22222222222223 Q 398.22222222222223 385.77777777777777 390.22222222222223 377.77777777777777 Q 382.22222222222223 369.77777777777777 369.77777777777777 369.77777777777777 L 312.8888888888889 369.77777777777777 L 284.44444444444446 369.77777777777777 L 284.44444444444446 331.55555555555554 L 284.44444444444446 331.55555555555554 Q 284.44444444444446 302.22222222222223 267.55555555555554 280 Q 250.66666666666666 257.77777777777777 222.22222222222223 248.88888888888889 L 195.55555555555554 240.88888888888889 L 195.55555555555554 240.88888888888889 Q 183.11111111111111 236.44444444444446 185.77777777777777 224 Q 190.22222222222223 211.55555555555554 202.66666666666666 214.22222222222223 L 230.22222222222223 221.33333333333334 L 230.22222222222223 221.33333333333334 Q 267.55555555555554 232.88888888888889 289.77777777777777 262.22222222222223 Q 312 292.44444444444446 312.8888888888889 331.55555555555554 L 312.8888888888889 344.8888888888889 L 312.8888888888889 344.8888888888889 L 362.6666666666667 316.44444444444446 L 362.6666666666667 316.44444444444446 L 368 312.8888888888889 L 368 312.8888888888889 L 458.6666666666667 312.8888888888889 L 458.6666666666667 312.8888888888889 Q 480.8888888888889 312 496 296.8888888888889 Q 511.1111111111111 281.77777777777777 512 259.55555555555554 Q 511.1111111111111 233.77777777777777 492.44444444444446 217.77777777777777 L 461.3333333333333 192.88888888888889 L 461.3333333333333 192.88888888888889 Q 455.1111111111111 187.55555555555554 448.8888888888889 184 Q 454.22222222222223 150.22222222222223 443.55555555555554 119.11111111111111 Q 433.77777777777777 88 409.77777777777777 63.111111111111114 L 409.77777777777777 63.111111111111114 Z M 126.22222222222223 402.6666666666667 Q 116.44444444444444 409.77777777777777 114.66666666666667 421.3333333333333 L 114.66666666666667 421.3333333333333 L 114.66666666666667 421.3333333333333 Q 112 432 118.22222222222223 442.6666666666667 Q 125.33333333333333 452.44444444444446 136.88888888888889 454.22222222222223 Q 147.55555555555554 456.8888888888889 158.22222222222223 450.6666666666667 L 199.11111111111111 423.1111111111111 L 199.11111111111111 423.1111111111111 L 155.55555555555554 383.1111111111111 L 155.55555555555554 383.1111111111111 L 126.22222222222223 402.6666666666667 L 126.22222222222223 402.6666666666667 Z M 426.6666666666667 241.77777777777777 Q 427.55555555555554 228.44444444444446 440.8888888888889 227.55555555555554 Q 454.22222222222223 228.44444444444446 455.1111111111111 241.77777777777777 Q 454.22222222222223 255.11111111111111 440.8888888888889 256 Q 427.55555555555554 255.11111111111111 426.6666666666667 241.77777777777777 L 426.6666666666667 241.77777777777777 Z" />
		</svg>
	),
	LoadingSpinner: ({ style }: { style?: React.CSSProperties }) => (
		<svg
			xmlns="http://www.w3.org/2000/svg"
			viewBox="0 0 100 100"
			preserveAspectRatio="xMidYMid"
			width="18"
			height="18"
			xmlnsXlink="http://www.w3.org/1999/xlink"
			style={style}
		>
			<g>
				<circle stroke-dasharray="164.93361431346415 56.97787143782138" r="35" stroke-width="10" stroke="currentColor" fill="none" cy="50" cx="50">
					<animateTransform keyTimes="0;1" values="0 50 50;360 50 50" dur="1s" repeatCount="indefinite" type="rotate" attributeName="transform" />
				</circle>
				<g />
			</g>
		</svg>
	),
	Empty: () => (
		<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
			<path
				fill="currentColor"
				d="m198.24 62.63l15.68-17.25a8 8 0 0 0-11.84-10.76L186.4 51.86A95.95 95.95 0 0 0 57.76 193.37l-15.68 17.25a8 8 0 1 0 11.84 10.76l15.68-17.24A95.95 95.95 0 0 0 198.24 62.63M48 128a80 80 0 0 1 127.6-64.25l-107 117.73A79.63 79.63 0 0 1 48 128m80 80a79.55 79.55 0 0 1-47.6-15.75l107-117.73A79.95 79.95 0 0 1 128 208"
			/>
		</svg>
	),
	ChromeExtension: () => (
		<svg
			xmlns="http://www.w3.org/2000/svg"
			style={{ fill: 'white', height: '18px' }}
			viewBox="0 0 20 20"
			width="20"
			height="20"
			fill="context-fill"
			fill-opacity="context-fill-opacity"
		>
			<path d="m15.5 5-2 0 0-1.873c0-1.594-1.183-2.961-2.693-3.112A3.003 3.003 0 0 0 7.5 2.999L7.5 5l-3 0A2.5 2.5 0 0 0 2 7.5l0 2.25c0 .69.56 1.25 1.25 1.25l1.623 0c.833 0 1.544.59 1.62 1.343a1.49 1.49 0 0 1-.379 1.163c-.285.314-.69.494-1.113.494L3.25 14C2.56 14 2 14.56 2 15.25l0 2.25A2.5 2.5 0 0 0 4.5 20l11 0a2.5 2.5 0 0 0 2.5-2.5l0-10A2.5 2.5 0 0 0 15.5 5zm1 12.7-.8.8-11.4 0-.8-.8 0-2.2 1.501 0c.846 0 1.657-.36 2.225-.988a3.01 3.01 0 0 0 .759-2.318C7.834 10.683 6.467 9.5 4.873 9.5L3.5 9.5l0-2.2.8-.8 3.7 0a1 1 0 0 0 1-1l0-2.501a1.502 1.502 0 0 1 1.657-1.492c.753.075 1.343.787 1.343 1.62L12 5.5a1 1 0 0 0 1 1l2.7 0 .8.8 0 10.4z" />
		</svg>
	),
};

export { MillenniumIcons, ConnectIcon, CoffeeIcon, ContrastIcon, CloudDownloadIcon, CctvIcon, RabbitIcon };
