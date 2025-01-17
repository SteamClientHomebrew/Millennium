import { DialogBody, DialogHeader, pluginSelf, showModal } from "@steambrew/client"
import { GenericConfirmDialog } from "../GenericDialog"
import React from "react";

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
}

const DateToLocalTime = (date: string) => {
    const dateTime = new Date(date)
    return dateTime.toLocaleString()
}

const RenderDownloadInformation: React.FC<DownloadInformationProps> = ({ props, targetAsset }) => (
    <GenericConfirmDialog>
        <DialogHeader> Download Information </DialogHeader>
        <DialogBody>
            <div className="DialogBodyText">
                <p className="updateInfoTidbit">
                    <b>Published By</b>
                    <div className="publisherInformation" style={{ display: "flex", gap: "10px" }}>
                        <img src={props.author.avatar_url} alt="Author Avatar" style={{ width: "20px", height: "20px" }} />
                        <div className="publisherInfo"> {props.author.login} </div>
                    </div>
                    <br />
                    <b>Version:</b> {props.tag_name} <br />
                    <b>Release Date:</b> {DateToLocalTime(props.published_at)} <br />
                    <b>Download Size:</b> {FormatBytes(targetAsset.size)} <br />
                    <b>Download Link:</b> <a href={props.html_url} target="_blank">{targetAsset.name}</a>
                </p>
            </div>
        </DialogBody>
    </GenericConfirmDialog>
)

export const ShowDownloadInformation = (props: any, targetAsset: any) => (
    showModal(
        <RenderDownloadInformation props={props} targetAsset={targetAsset} />,
        pluginSelf.millenniumUpdaterWindow,
        {
            strTitle: "Download Information",
            popupHeight: 340, popupWidth: 475,
        }
    )
)