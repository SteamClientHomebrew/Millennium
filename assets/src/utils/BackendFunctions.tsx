import { callable, showModal, ConfirmModal, pluginSelf } from '@steambrew/client';

export enum UIReloadProps {
	None,
	Force,
	Prompt,
}

// @ts-ignore (Didn't type this yet)
const Localize = (token: string): string => LocalizationManager.LocalizeString(token);

const PromptReload = (onOK: () => void) => {
	const title = Localize('#Settings_RestartRequired_Title');
	const description = Localize('#Settings_RestartRequired_Description');
	const buttonText = Localize('#Settings_RestartNow_ButtonText');

	showModal(<ConfirmModal strTitle={title} strDescription={description} strOKButtonText={buttonText} onOK={onOK} />, pluginSelf.windows['Millennium'], {
		bNeverPopOut: false,
	});
};

export const ChangeActiveTheme = async (themeName: string, reloadProps: UIReloadProps) => {
	const result = await callable<[{ theme_name: string }]>('cfg.change_theme')({ theme_name: themeName });

	reloadProps === UIReloadProps.Force && SteamClient.Browser.RestartJSContext();
	reloadProps === UIReloadProps.Prompt && PromptReload(() => SteamClient.Browser.RestartJSContext());

	return result;
};
