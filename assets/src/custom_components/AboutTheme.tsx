import {
	Classes,
	DialogBody,
	DialogBodyText,
	DialogButton,
	DialogFooter,
	DialogHeader,
	DialogSubHeader,
	Millennium,
	pluginSelf,
} from '@steambrew/client';
import { ThemeItem } from '../types';
import React from 'react';
import { locale } from '../locales';
import { devClasses, settingsClasses } from '../classes';
import { FakeFriend } from './FakeFriend';
import { CreatePopup } from '../components/ISteamComponents';
import Styles from '../styles';

class AboutThemeRenderer extends React.Component<any> {
	activeTheme: ThemeItem;

	constructor(props: any) {
		super(props);

		this.activeTheme = pluginSelf.activeTheme;
	}

	RenderDeveloperProfile = () => {
		const OpenDeveloperProfile = () => {
			this.activeTheme?.data?.github?.owner &&
				SteamClient.System.OpenInSystemBrowser(`https://github.com/${this.activeTheme?.data?.github?.owner}/`);
		};

		return (
			<>
				<style>
					{`
                .${Classes.FakeFriend}.online:hover {
                    cursor: pointer !important;
                }
                
                .${Classes.avatarHolder}.avatarHolder.no-drag.Medium.online,
                .online.${devClasses.noContextMenu}.${devClasses.twoLine} {
                    pointer-events: none;
                }`}
				</style>

				<FakeFriend
					eStatus="online"
					strAvatarURL={
						this.activeTheme?.data?.github?.owner
							? `https://github.com/${this.activeTheme?.data?.github?.owner}.png`
							: 'https://i.pinimg.com/736x/98/1d/6b/981d6b2e0ccb5e968a0618c8d47671da.jpg'
					}
					strGameName={`✅ ${locale.aboutThemeVerifiedDev}`}
					strPlayerName={this.activeTheme?.data?.github?.owner ?? this.activeTheme?.data?.author ?? locale.aboutThemeAnonymous}
					onClick={OpenDeveloperProfile}
				/>
			</>
		);
	};

	RenderDescription = () => {
		return (
			<>
				<DialogSubHeader className={settingsClasses.SettingsDialogSubHeader}>{locale.aboutThemeTitle}</DialogSubHeader>
				<DialogBodyText className={Classes.FriendsDescription}>
					{this.activeTheme?.data?.description ?? locale.itemNoDescription}
				</DialogBodyText>
			</>
		);
	};

	RenderInfoRow = () => {
		const themeOwner = this.activeTheme?.data?.github?.owner;
		const themeRepo = this.activeTheme?.data?.github?.repo_name;
		const kofiDonate = this.activeTheme?.data?.funding?.kofi;

		const ShowSource = () => {
			SteamClient.System.OpenInSystemBrowser(`https://github.com/${themeOwner}/${themeRepo}`);
		};

		const OpenDonateDefault = () => {
			SteamClient.System.OpenInSystemBrowser(`https://ko-fi.com/${kofiDonate}`);
		};

		const ShowInFolder = () => {
			Millennium.callServerMethod('Millennium.steam_path')
				.then((result: any) => {
					pluginSelf.connectionFailed = false;
					return result;
				})
				.then((path: string) => {
					console.log(path);
					SteamClient.System.OpenLocalDirectoryInSystemExplorer(`${path}/steamui/skins/${this.activeTheme.native}`);
				});
		};

		const UninstallTheme = () => {
			Millennium.callServerMethod('uninstall_theme', {
				owner: this.activeTheme?.data?.github?.owner,
				repo: this.activeTheme?.data?.github?.repo_name,
			})
				.then((result: any) => {
					pluginSelf.connectionFailed = false;
					return result;
				})
				.then((raw: string) => {
					const message = JSON.parse(raw);
					console.log(message);

					SteamClient.Browser.RestartJSContext();
				});
		};

		return (
			<DialogFooter>
				<div className="DialogTwoColLayout _DialogColLayout Panel">
					{/* {kofiDonate && <DialogButton onClick={OpenDonateDefault}>Donate</DialogButton>} */}
					{themeOwner && themeRepo && <DialogButton onClick={ShowSource}>{locale.viewSourceCode}</DialogButton>}
					<DialogButton onClick={ShowInFolder}>{locale.showInFolder}</DialogButton>
					<DialogButton onClick={UninstallTheme}>{locale.uninstall}</DialogButton>
				</div>
			</DialogFooter>
		);
	};

	CreateModalBody = () => {
		return (
			<div className="ModalPosition" tabIndex={0}>
				<div className="ModalPosition_Content">
					<div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
						<div className="DialogContent_InnerWidth">
							<DialogHeader>
								{this.activeTheme?.data?.name ?? this.activeTheme?.native}
								<span className="MillenniumAboutTheme_Version">{this.activeTheme?.data?.version}</span>
							</DialogHeader>
							<Styles />
							<DialogBody>
								<this.RenderDeveloperProfile />
								<this.RenderDescription />
							</DialogBody>
							<this.RenderInfoRow />
						</div>
					</div>
				</div>
			</div>
		);
	};

	render() {
		return <this.CreateModalBody />;
	}
}

export const SetupAboutRenderer = (active: string) => {
	const params: any = {
		title: locale.aboutThemeTitle + ' ' + active,
		popup_class: 'fullheight',
		body_class: 'fullheight ModalDialogBody DesktopUI ',
		html_class: 'client_chat_frame fullheight ModalDialogPopup ',
		eCreationFlags: 274,
		window_opener_id: 1,
		dimensions: { width: 450, height: 375 },
		replace_existing_popup: false,
	};

	const popupWND = new CreatePopup(AboutThemeRenderer, 'about_theme', params);
	popupWND.Show();
};
