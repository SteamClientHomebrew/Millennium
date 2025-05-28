import { DialogButton, IconsModule, SteamSpinner } from '@steambrew/client';
import { useState } from 'react';

interface ErrorModalProps {
	header: string;
	body: string;

	options?: {
		buttonText?: string;
		onClick?: () => void;
	};
}

export const ErrorModal: React.FC<ErrorModalProps> = ({ header, body, options }) => {
	const [callBackRunning, setCallBackRunning] = useState(false);

	const DelegateCallback = async (callback: () => void) => {
		if (callBackRunning) {
			return;
		}

		setCallBackRunning(true);
		await callback();
		setCallBackRunning(false);
	};

	return (
		<div className="MillenniumErrorModal_Container">
			<IconsModule.Caution width="64" />

			<div className="MillenniumErrorModal_Header">{header}</div>
			<div className="MillenniumErrorModal_Text">{body}</div>

			{options && (
				<DialogButton disabled={callBackRunning} className="MillenniumErrorModal_Button" onClick={DelegateCallback.bind(null, options.onClick)}>
					{options.buttonText}
				</DialogButton>
			)}
		</div>
	);
};
