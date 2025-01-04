import React from 'react';
import { PluginComponent } from '../types';
import { DialogBody, MillenniumModuleSettings, MillenniumSettingTabs, ModalPosition, SettingMetadata, SidebarNavigation, SidebarNavigationPage } from '@steambrew/client';
import { PluginField } from './PluginField';
import { settingsClasses } from '../classes';

type PluginSettingsProps = {
    plugin: PluginComponent;
}

export type ComponentProps = {
    settingsModule: MillenniumModuleSettings;
    propertyKey: string;
    metadata: SettingMetadata;
}

export default class RenderPluginSettings extends React.Component<PluginSettingsProps> {
    plugin: PluginComponent;
    pluginSettings: MillenniumModuleSettings | MillenniumSettingTabs;

    constructor(props: PluginSettingsProps) {
        super(props);
        this.plugin = props.plugin;
        this.pluginSettings = window.PLUGIN_LIST[this.plugin.data.name].settings;
    }

    RenderPage: React.FC<{ settingsModule: MillenniumModuleSettings }> = ({settingsModule}) => {
        return (
            <DialogBody>
                {
                    Object.entries(settingsModule.metadata).map(([key, value]) => (
                        <PluginField settingsModule={settingsModule} propertyKey={key} metadata={value}/>
                    ))
                }
            </DialogBody>
        );
    };

    render() {
        let pages: SidebarNavigationPage[] = [];

        if (this.pluginSettings instanceof MillenniumSettingTabs) {
            for (let metadataKey in this.pluginSettings.metadata) {
                const metadata = this.pluginSettings.metadata[metadataKey];
                const settingsModule = this.pluginSettings[metadataKey];
                if (!(settingsModule instanceof MillenniumModuleSettings)) {
                    throw new Error(`Expected MillenniumModuleSettings but received ${typeof settingsModule} in MillenniumSettingTabs object`);
                }

                //TODO: find a way to fix the bug where toggle values are kept between tabs
                // using a route on the page seems to fix it but fully breaks the library
                // probably need to do hook into the react router like decky does
                pages.push({
                    ...metadata,
                    content: <this.RenderPage settingsModule={settingsModule}/>,
                });
            }
        } else if (this.pluginSettings instanceof MillenniumModuleSettings) {
            pages.push({
                title: `${this.plugin.data.common_name} settings`,
                content: <this.RenderPage settingsModule={this.pluginSettings}/>,
            });
        }

        const className = `${settingsClasses.SettingsModal} ${settingsClasses.DesktopPopup}`;
        const title = <>{this.plugin.data.common_name}<br/>settings</>;
        const hasMultiplePages = this.pluginSettings instanceof MillenniumSettingTabs;

        return (
            <ModalPosition>
                {!hasMultiplePages && (
                    <style>{`
                        .PageListColumn {
                            display: none !important;
                        }
                        .DialogContentTransition {
                            max-width: unset !important;
                        }
                    `}</style>
                )}

                {/* @ts-ignore: className hasn't been added to DFL yet */}
                <SidebarNavigation className={className} pages={pages} title={title}>

                </SidebarNavigation>
            </ModalPosition>
        );
    }
}
