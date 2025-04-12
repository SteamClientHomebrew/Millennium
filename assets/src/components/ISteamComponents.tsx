import { Classes } from '@steambrew/client';
import React, { CSSProperties, ReactNode } from 'react';
import { findModuleExport } from '@steambrew/client';
import ReactDOM from 'react-dom';
import { FC } from 'react';
import { fieldClasses } from '../classes';

interface BBCodeParserProps {
	bShowShortSpeakerInfo?: boolean;
	event?: any;
	languageOverride?: any;
	showErrorInfo?: boolean;
	text?: string;
}

export const BBCodeParser: FC<BBCodeParserProps> = findModuleExport(
	(m) => typeof m === 'function' && m.toString().includes('this.ElementAccumulator'),
);

export const CreatePopupBase: any = findModuleExport((m) => typeof m === 'function' && m?.toString().includes('CreatePopup(this.m_strName'));

export interface RenderProps {
	_window: Window;
}

export class CreatePopup extends CreatePopupBase {
	constructor(component: ReactNode, strPopupName: string, options: any) {
		super(strPopupName, options);
		this.component = component;
	}

	Show() {
		super.Show();

		const RenderComponent: React.FC<RenderProps> = ({ _window }) => {
			return (
				<div className="PopupFullWindow">
					<TitleBar popup={_window} hideMin={false} hideMax={false} hideActions={false} />
					<this.component />
				</div>
			);
		};
		ReactDOM.render(<RenderComponent _window={super.window} />, super.root_element);
	}

	SetTitle() {
		if (this.m_popup && this.m_popup.document) {
			this.m_popup.document.title = 'WINDOW';
		}
	}

	Render(_window: Window, _element: HTMLElement) {}

	OnClose() {}

	OnLoad() {
		if (this.strPopupName === 'about_theme') {
			const element: HTMLElement = (this.m_popup.document as Document).querySelector('.DialogContent_InnerWidth');
			const height = element?.getBoundingClientRect()?.height;

			this.m_popup.SteamClient?.Window?.ResizeTo(450, height + 48, true);
		}
		this.m_popup.SteamClient.Window.ShowWindow();
	}
}

interface TitleBarProps {
	bOSX?: boolean;
	bForceWindowFocused?: boolean;
	children?: ReactNode;
	className?: string;
	extraActions?: ReactNode;
	hideActions?: boolean;
	hideClose?: boolean;
	hideMin?: boolean;
	hideMax?: boolean;
	onClose?: (e: PointerEvent) => void;
	onMaximize?: () => void;
	onMinimize?: (e: PointerEvent) => void;
	popup?: Window;
	style?: CSSProperties;
}

export const TitleBar: React.FC<TitleBarProps> = findModuleExport(
	(m) => typeof m === 'function' && m.toString().includes('className:"title-area-highlight"'),
);

interface SettingsDialogSubHeaderProps {
	children: ReactNode;
}

export const SettingsDialogSubHeader: React.FC<SettingsDialogSubHeaderProps> = ({ children }) => (
	<div className="SettingsDialogSubHeader">{children}</div>
);

export const Separator: React.FC = () => <div className={fieldClasses.StandaloneFieldSeparator} />;
