import { FC, memo, ReactNode } from 'react';
import { findClassModule } from '../class-mapper';
import Logger from '../logger';
import { Navigation } from '../modules/Router';
import { joinClassNames } from '../utils';
import { Focusable } from './Focusable';

const logger = new Logger('ToastRenderer');

// TODO there are more of these
export enum ToastLocation {
	/** Big Picture popup toasts */
	GAMEPADUI_POPUP = 1,
	/** QAM Notifications tab */
	GAMEPADUI_QAM = 3,
}

export interface ToastData {
	title: ReactNode;
	body: ReactNode;
	subtext?: ReactNode;
	logo?: ReactNode;
	icon?: ReactNode;
	timestamp?: Date;
	onClick?: () => void;
	className?: string;
	contentClassName?: string;
	/** ms before toast popup is hidden. defaults to 5 seconds */
	duration?: number;
	/** ms before toast is removed from the tray. Valve's logic will always remove all toasts 48h after they are first viewed regardless of this value */
	expiration?: number;
	critical?: boolean;
	eType?: number;
	sound?: number;
	/** Hidden 10min after first viewed */
	showNewIndicator?: boolean;
	playSound?: boolean;
	showToast?: boolean;
}

interface ToastProps {
	toast: ToastData;
	newIndicator?: boolean;
}

interface ToastRendererProps extends ToastProps {
	location: ToastLocation;
}

const templateClasses = findClassModule((m) => m.ShortTemplate) || {};

// These are memoized as they like to randomly rerender

const GamepadUIPopupToast: FC<Omit<ToastProps, 'newIndicator'>> = memo(({ toast }) => {
	return (
		<div
			style={{ '--toast-duration': `${toast.duration}ms` } as React.CSSProperties}
			onClick={toast.onClick}
			className={joinClassNames(templateClasses.ShortTemplate, toast.className || '', 'MillenniumGamepadUIPopupToast')}
		>
			{toast.logo && <div className={templateClasses.StandardLogoDimensions}>{toast.logo}</div>}
			<div className={joinClassNames(templateClasses.Content, toast.contentClassName || '')}>
				<div className={templateClasses.Header}>
					{toast.icon && <div className={templateClasses.Icon}>{toast.icon}</div>}
					<div className={templateClasses.Title}>{toast.title}</div>
				</div>
				<div className={templateClasses.Body}>{toast.body}</div>
			</div>
		</div>
	);
});

const GamepadUIQAMToast: FC<ToastProps> = memo(({ toast, newIndicator }) => {
	// The fields aren't mismatched, the logic for these is just a bit weird.
	return (
		<Focusable
			onActivate={() => {
				toast.onClick?.();
				Navigation.CloseSideMenus();
			}}
			className={joinClassNames(templateClasses.StandardTemplateContainer, toast.className || '', 'MillenniumGamepadUIQAMToast')}
		>
			<div className={templateClasses.StandardTemplate}>
				{toast.logo && <div className={templateClasses.StandardLogoDimensions}>{toast.logo}</div>}
				<div className={joinClassNames(templateClasses.Content, toast.contentClassName || '')}>
					<div className={templateClasses.Header}>
						{toast.icon && <div className={templateClasses.Icon}>{toast.icon}</div>}
						{toast.title && <div className={templateClasses.Title}>{toast.title}</div>}
						{/* timestamp should always be defined by toaster */}
						{/* TODO check how valve does this */}
						{toast.timestamp && <div className={templateClasses.Timestamp}>{toast.timestamp.toLocaleTimeString(undefined, { timeStyle: 'short' })}</div>}
					</div>
					{toast.body && <div className={templateClasses.StandardNotificationDescription}>{toast.body}</div>}
					{toast.subtext && <div className={templateClasses.StandardNotificationSubText}>{toast.subtext}</div>}
				</div>
				{newIndicator && (
					<div className={templateClasses.NewIndicator}>
						<svg xmlns="http://www.w3.org/2000/svg" width="50" height="50" viewBox="0 0 50 50" fill="none">
							<circle fill="currentColor" cx="25" cy="25" r="25"></circle>
						</svg>
					</div>
				)}
			</div>
		</Focusable>
	);
});

/** @hide @internal This is not meant for external use */
export const ToastRenderer: FC<ToastRendererProps> = memo(({ toast, location, newIndicator }) => {
	switch (location) {
		default:
			logger.warn(`Toast UI not implemented for location ${location}! Falling back to GamepadUIQAMToast.`);
			return <GamepadUIQAMToast toast={toast} newIndicator={false} />;
		case ToastLocation.GAMEPADUI_POPUP:
			return <GamepadUIPopupToast toast={toast} />;
		case ToastLocation.GAMEPADUI_QAM:
			return <GamepadUIQAMToast toast={toast} newIndicator={newIndicator} />;
	}
});

export default ToastRenderer;
