import {
    ConfirmModal,
    Dropdown,
    DialogButton,
    pluginSelf,
    Toggle,
    showModal,
    DropdownOption,
    DialogControlsSection,
    DialogBodyText,
    Field,
} from '@steambrew/client'

import { useState } from 'react'
import { locale } from '../locales'
import { settingsClasses } from '../classes'
import { SettingsDialogSubHeader } from '../components/SettingsDialogSubHeader'

declare global {
    interface Window {
        LastReportTime?: number;
    }
}

const MultiLineTextField = (props: { label: string, value: string, onChange: (value: string) => void }) => {
    return (
        <div className="DialogInputLabelGroup _DialogLayout DialogRequirementLabel">
            <label>
                <div className="DialogLabel">
                    {props.label}
                    <span className="DialogInputRequirementLabel" />
                </div>
                <div className="DialogInput_Wrapper _DialogLayout Panel">
                    <textarea
                        className="DialogInput DialogInputPlaceholder DialogTextInputBase Focusable"
                        tabIndex={0}
                        value={props.value}
                        onChange={(e) => props.onChange(e.target.value)}
                    />
                </div>
            </label>
        </div>
    )
}

const ShowErrorMessage = (message: string) => {
    showModal(
        <ConfirmModal
            strTitle={locale.errorMessageTitle}
            strDescription={message}
        />,
        pluginSelf.millenniumSettingsWindow,
        {
            bNeverPopOut: false,
        },
    );
}

const UserPromptBugReport = () => {
    showModal(
        <ConfirmModal
            strTitle={"Successfully Submitted Bug Report!"}
            strDescription={"Thanks submitting a report; we're constantly trying to make Millennium better!"}
        />,
        pluginSelf.millenniumSettingsWindow,
        {
            bNeverPopOut: false,
        },
    );
}

enum OSType {
    Windows,
    Linux
}

interface BugReportProps {
    issueDescription: string,
    stepsToReproduce: string,
    selectedOS: OSType
}

const FormatBugReport = async (report: BugReportProps) => {
    const systemInfo = await SteamClient.System.GetSystemInfo()

    return `
## Bug Report

Millennium Version: \`${pluginSelf.version}\`
Steam Path: \`${pluginSelf.steamPath}\`

**Steam Version:**
* Client: ${systemInfo?.nSteamVersion}
* API: ${systemInfo?.sSteamAPI}

**Issue Description:**
${report.issueDescription}

**Steps to Reproduce:**
${report.stepsToReproduce}

**Operating System:**
* Type: ${OSType[report.selectedOS]}
* Name: ${systemInfo?.sOSName} 

**Submit Date (UTC):**
${new Date().toUTCString()}
    `
}

export const BugReportViewModal: React.FC = () => {

    const operatingSystems: DropdownOption[] = [
        { label: "Windows", data: OSType.Windows },
        { label: "Linux", data: OSType.Linux }
    ];

    const [selectedOS, setSelectedOS] = useState<OSType>(operatingSystems[0].data as OSType);
    const [notCausedByTheme, setNotCausedByTheme] = useState<boolean>(false);
    const [notCausedByPlugin, setNotCausedByPlugin] = useState<boolean>(false);

    const [issueDescription, setIssueDescription] = useState<string>("");
    const [stepsToReproduce, setStepsToReproduce] = useState<string>("");

    const SubmitPluginReport = async () => {

        const jsonReport: BugReportProps = {
            issueDescription,
            stepsToReproduce,
            selectedOS
        }

        if (!notCausedByTheme && !notCausedByPlugin) {
            ShowErrorMessage(locale.errorSubmitIssueNotValid)
            return
        }

        if (issueDescription.length < 10) {
            ShowErrorMessage(locale.errorSubmitIssueNoDescription)
            return
        }

        if (stepsToReproduce.length < 10) {
            ShowErrorMessage(locale.errorSubmitIssueNoSteps)
            return
        }

        const lastReportTime = window.LastReportTime ?? 0

        if (new Date().getTime() - lastReportTime < 600000) {
            ShowErrorMessage(locale.errorSubmitIssueTooFrequent)
            return
        }

        /** No, you're not a hacker for finding this, and we didn't accidentally leave it. No point putting it in an .env variable as its visible from DOM inspector regardless */
        const webhookURL = "https://discord.com/api/webhooks/1326663682343702549/-T-Gw1wKVltJ3Srwk2u7OzV_QYBOKy1EzgbAtPIhFTEZeysSthKtY3D5sooPC7rHu9-J"

        fetch(webhookURL, {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify({
                content: await FormatBugReport(jsonReport),
                username: "Bug Report",
            }),
        })
            .then((response) => {
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                console.log("Message sent successfully!");
                UserPromptBugReport()

                window.LastReportTime = new Date().getTime()
            })
            .catch((error) => {
                console.error("Error sending message:", error);
            });
    }

    return (
        <>
            <style>
                {`
                .DialogDropDown._DialogInputContainer.Panel.Focusable { min-width: max-content !important; }
                button.millenniumIconButton {
                    padding: 9px 10px !important; 
                    margin: 0 !important; 
                    margin-right: 10px !important;
                    display: flex;
                    width: auto;
                }
                textarea.DialogInput.DialogInputPlaceholder.DialogTextInputBase.Focusable {
                    width: 100% !important;
                }
                textarea.DialogInput.DialogInputPlaceholder.DialogTextInputBase.Focusable:focus-visible {
                    margin-left: 2px !important;
                }
            `}
            </style>

            <Field
                label={"My issue is not caused by an active theme, and occurs with the vanilla Steam UI."}
                description={""}
            >
                <Toggle value={notCausedByTheme} onChange={(value) => { setNotCausedByTheme(value) }} />
            </Field>

            <Field
                label={"My issue is not caused by an active plugin, and occurs with no custom plugins enabled."}
                description={""}
            >
                <Toggle value={notCausedByPlugin} onChange={(value) => { setNotCausedByPlugin(value) }} />
            </Field>

            <DialogControlsSection>
                <SettingsDialogSubHeader>Bug Report Form</SettingsDialogSubHeader>
                <DialogBodyText className='_3fPiC9QRyT5oJ6xePCVYz8'>Fill out the following to the best of your abilities, the more information the better!</DialogBodyText>

                <MultiLineTextField label={"Please describe the issue you are experiencing."} value={issueDescription} onChange={(value) => { setIssueDescription(value) }} />
                <MultiLineTextField label={"Steps to reproduce bug."} value={stepsToReproduce} onChange={(value) => { setStepsToReproduce(value) }} />

                <Field
                    label={"Operating System"}
                    description={"Please select the operating system you are using."}
                >
                    <Dropdown
                        rgOptions={operatingSystems}
                        selectedOption={0}
                        contextMenuPositionOptions={{ bMatchWidth: false }}
                        strDefaultLabel={operatingSystems[0].label as string}
                        onChange={(selection) => { setSelectedOS(selection.data) }}
                    />
                </Field>
            </DialogControlsSection>

            <DialogButton onClick={() => { SubmitPluginReport() }} style={{ width: "unset", marginTop: "20px" }} className={settingsClasses.SettingsDialogButton}>Submit Report</DialogButton>
        </>
    )
}