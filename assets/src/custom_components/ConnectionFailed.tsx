import { DialogButton, IconsModule, pluginSelf } from "@millennium/ui";

export const ConnectionFailed = () => {

    const OpenLogsFolder = () => {
        const logsPath = [pluginSelf.steamPath, "ext", "data", "logs"].join("/");
        SteamClient.System.OpenLocalDirectoryInSystemExplorer(logsPath);
    }

    return (
        <div className="__up-to-date-container" style={{
            display: "flex", flexDirection: "column", alignItems: "center", height: "100%", justifyContent: "center"
        }}>
            <IconsModule.Caution width="64" />
            
            <div className="__up-to-date-header" style={{marginTop: "20px", color: "white", fontWeight: "500", fontSize: "15px"}}>Failed to connect to Millennium!</div>
            <p style={{ fontSize: "12px", color: "grey", textAlign: "center", maxWidth: "76%" }}>This issue isn't network related, you're most likely missing a file millennium needs, or are experiencing an unexpected bug.</p>

            <DialogButton onClick={OpenLogsFolder} style={{width: "50%", marginTop: "20px"}}>Open Logs Folder</DialogButton>
        </div>
    );
}