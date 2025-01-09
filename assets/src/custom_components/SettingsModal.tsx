import React, { useEffect, useState } from "react"
import {
    Classes,
    DialogBody,
    IconsModule,
    ModalPosition,
    pluginSelf,
    Router,
    SidebarNavigation,
    type SidebarNavigationPage,
} from "@steambrew/client";
import { settingsClasses } from "../classes";
import * as CustomIcons from "./CustomIcons";
import { PluginViewModal } from "../tabs/Plugins";
import { ThemeViewModal } from "../tabs/Themes";
import { UpdatesViewModal } from "../tabs/Updates";
import { locale } from "../locales";
import { BugReportViewModal } from "../tabs/BugReport";
import { LogsViewModal } from "../tabs/Logs";

export class MillenniumSettings extends React.Component {

    render() {
        const className = `${settingsClasses.SettingsModal} ${settingsClasses.DesktopPopup} .millennium-settings`;

        const pages: SidebarNavigationPage[] = [
            {
                visible: true,
                title: locale.settingsPanelThemes,
                icon: <CustomIcons.Themes />,
                content: (
                    <DialogBody className={Classes.SettingsDialogBodyFade}>
                        <ThemeViewModal />
                    </DialogBody>
                ),
                // route: "/settings/millennium/themes",
            },
            {
                visible: true,
                title: locale.settingsPanelPlugins,
                icon: <CustomIcons.Plugins />,
                content: (
                    <DialogBody className={Classes.SettingsDialogBodyFade}>
                        <PluginViewModal />
                    </DialogBody>
                ),
                // route: "/settings/millennium/plugins",
            },
            {
                visible: true,
                title: locale.settingsPanelUpdates,
                icon: <IconsModule.Update />,
                content: (
                    <DialogBody className={Classes.SettingsDialogBodyFade}>
                        <UpdatesViewModal />
                    </DialogBody>
                ),
                // route: "/settings/millennium/updates",
            },
            {
                visible: true,
                title: locale.settingsPanelBugReport,
                icon: <IconsModule.BugReport />,
                content: (
                    <DialogBody className={Classes.SettingsDialogBodyFade}>
                        <BugReportViewModal />
                    </DialogBody>
                ),
                // route: "/settings/millennium/bugreport",s
            },
            {
                visible: true,
                title: locale.settingsPanelLogs,
                icon: <IconsModule.TextCodeBlock />,
                content: (
                    <DialogBody className={Classes.SettingsDialogBodyFade}>
                        <LogsViewModal />
                    </DialogBody>
                ),
                // route: "/settings/millennium/logs",
            },
        ];
        const title = `Millennium ${pluginSelf.version}`;

        console.log(IconsModule)

        return (
            <ModalPosition>
                <style>
                    {`.DialogBody { margin-bottom: 48px; }
                    input.colorPicker { margin-left: 10px !important; border: unset !important; min-width: 38px; width: 38px !important; height: 38px; !important; background: transparent; padding: unset !important; }`}
                </style>

                {/* @ts-ignore */}
                <SidebarNavigation className={className} pages={pages} title={title} />
            </ModalPosition>
        );
    }
}