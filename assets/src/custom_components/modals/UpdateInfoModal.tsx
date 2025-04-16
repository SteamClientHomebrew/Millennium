import { DialogBody, DialogHeader, pluginSelf, showModal } from '@steambrew/client';
import { GenericConfirmDialog } from '../GenericDialog';
import React, { FC, ReactNode } from 'react';
import Styles from '../../styles';

export interface DownloadInformationProps {
	// Github RESTFUL API types, Im too lazy to type it.
	props: any;
	targetAsset: any;
}

const FormatBytes = (bytes: number, decimals = 2) => {
	if (bytes <= 0) {
		return '0 Bytes';
	}

	const kilobyteMultiplier = 1024;
	const adjustedDecimals = decimals < 0 ? 0 : decimals;
	const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'];

	const i = Math.floor(Math.log(bytes) / Math.log(kilobyteMultiplier));
	return parseFloat((bytes / Math.pow(kilobyteMultiplier, i)).toFixed(adjustedDecimals)) + ' ' + sizes[i];
};

const DateToLocalTime = (date: string) => {
	const dateTime = new Date(date);
	return dateTime.toLocaleString();
};

interface InfoFieldProps {
	children: ReactNode;
	name: string;
}

const InfoField: FC<InfoFieldProps> = ({ children, name }) => (
	<div className="MillenniumDownloadInfo_Field">
		<b>{name}:</b>
		{children}
	</div>
);

const RenderDownloadInformation: React.FC<DownloadInformationProps> = ({ props, targetAsset }) => (
	<GenericConfirmDialog>
		<DialogHeader> Download Information </DialogHeader>
		<Styles />
		<DialogBody>
			<InfoField name="Published By">
				<div className="MillenniumDownloadInfo_Publisher">
					<img className="MillenniumDownloadInfo_PublisherAvatar" src={props.author.avatar_url} />
					{props.author.login}
				</div>
			</InfoField>
			<InfoField name="Version">{props.tag_name}</InfoField>
			<InfoField name="Release Date">{DateToLocalTime(props.published_at)}</InfoField>
			<InfoField name="Download Size">{FormatBytes(targetAsset.size)}</InfoField>
			<InfoField name="Download Link">
				<a href={props.html_url} target="_blank">
					{targetAsset.name}
				</a>
			</InfoField>
		</DialogBody>
	</GenericConfirmDialog>
);

export const ShowDownloadInformation = (props: any, targetAsset: any) =>
	showModal(<RenderDownloadInformation props={props} targetAsset={targetAsset} />, pluginSelf?.windows?.['Millennium Updater'], {
		strTitle: 'Download Information',
		popupHeight: 340,
		popupWidth: 475,
	});
