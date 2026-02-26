import { FC, ReactNode } from 'react';

import { findSP } from '../utils';
import { Export, findModule, findModuleDetailsByExport, findModuleExport } from '../webpack';

// All of the popout options + strTitle are related. Proper usage is not yet known...
export interface ShowModalProps {
	browserContext?: unknown;
	bForcePopOut?: boolean;
	bHideActionIcons?: boolean;
	bHideMainWindowForPopouts?: boolean;
	bNeverPopOut?: boolean;
	fnOnClose?: () => void; // Seems to be the same as "closeModal" callback, but only when the modal is a popout. Will no longer work after "Update" invocation!
	popupHeight?: number;
	popupWidth?: number;
	promiseRenderComplete?: Promise<void>; // Invoked once the render is complete. Currently, it seems to be used as image loading success/error callback...
	strTitle?: string;
}

export interface ShowModalResult {
	// This method will not invoke any of the variations of "closeModal" callbacks!
	Close: () => void;

	// This method will replace the modal element completely and will not update the callback chains,
	// meaning that "closeModal" and etc. will not automatically close the modal anymore (also "fnOnClose"
	// will not be even called upon close anymore)! You have to manually call the "Close" method when, for example,
	// the "closeModal" is invoked in the newly updated modal:
	//   <ModalRoot closeModal={() => { console.log("ABOUT TO CLOSE"); showModalRes.Close(); }} />
	Update: (modal: ReactNode) => void;
}

const showModalRaw: (
	modal: ReactNode,
	parent?: EventTarget,
	title?: string,
	props?: ShowModalProps,
	unknown1?: unknown,
	hideActions?: { bHideActions?: boolean },
	modalManager?: unknown,
) => ShowModalResult = findModuleExport((e: Export) => typeof e === 'function' && e.toString().includes('props.bDisableBackgroundDismiss') && !e?.prototype?.Cancel);

/** @component Popup Modals */
export const showModal = (
	modal: ReactNode,
	parent?: EventTarget,
	props: ShowModalProps = {
		strTitle: 'Millennium Dialog',
		bHideMainWindowForPopouts: false,
	},
): ShowModalResult => {
	return showModalRaw(modal, parent || findSP() || window, props.strTitle, props, undefined, {
		bHideActions: props.bHideActionIcons,
	});
};

export interface ModalRootProps {
	children?: ReactNode;
	onCancel?(): void;
	closeModal?(): void;
	onOK?(): void;
	onEscKeypress?(): void;
	className?: string;
	modalClassName?: string;
	bAllowFullSize?: boolean;
	bDestructiveWarning?: boolean;
	bDisableBackgroundDismiss?: boolean;
	bHideCloseIcon?: boolean;
	bOKDisabled?: boolean;
	bCancelDisabled?: boolean;
}

export interface ConfirmModalProps extends ModalRootProps {
	onMiddleButton?(): void; // setting this prop will enable the middle button
	strTitle?: ReactNode;
	strDescription?: ReactNode;
	strOKButtonText?: ReactNode;
	strCancelButtonText?: ReactNode;
	strMiddleButtonText?: ReactNode;
	bAlertDialog?: boolean; // This will open a modal with only OK button enabled
	bMiddleDisabled?: boolean;
}
/** @component React Components */
export const ConfirmModal = findModuleExport(
	(e: Export) => e?.toString()?.includes('bUpdateDisabled') && e?.toString()?.includes('closeModal') && e?.toString()?.includes('onGamepadCancel'),
) as FC<ConfirmModalProps>;

/** @component React Components */
export const ModalRoot =
	// new
	findModuleExport((e: Export) => typeof e === 'function' && e.toString().includes('Either closeModal or onCancel should be passed to GenericDialog. Classes: ')) ||
	// old
	Object.values(
		findModule((m: any) => {
			if (typeof m !== 'object') return false;

			for (let prop in m) {
				if (m[prop]?.m_mapModalManager && Object.values(m)?.find((x: any) => x?.type)) {
					return true;
				}
			}

			return false;
		}) || {},
	)?.find((x: any) => x?.type?.toString?.()?.includes('((function(){')) as FC<ModalRootProps>;

interface SimpleModalProps {
	active?: boolean;
	children: ReactNode;
}

const [ModalModule, _ModalPosition] = findModuleDetailsByExport((e: Export) => e?.toString().includes('.ModalPosition'), 5);

const ModalModuleProps = ModalModule ? Object.values(ModalModule) : [];

/** @component React Components */
export const SimpleModal = ModalModuleProps.find((prop) => {
	const string = prop?.toString();
	return string?.includes('.ShowPortalModal()') && string?.includes('.OnElementReadyCallbacks.Register(');
}) as FC<SimpleModalProps>;

/** @component React Components */
export const ModalPosition = _ModalPosition as FC<SimpleModalProps>;
