import { Millennium, pluginSelf } from "@millennium/ui"
import { SettingsProps } from "./types"

export let SettingsStore: SettingsProps = pluginSelf.SettingsStore

export const Settings = {

    FetchAllSettings: () => {
        return new Promise<SettingsProps>(async (resolve: any, _reject: any) => {

            const settingsStore: SettingsProps = JSON.parse(
                await Millennium.callServerMethod("get_load_config")
                .then((result: any) => {
                    pluginSelf.connectionFailed = false
                    return result
                })
                .catch((_: any) => {
                    console.error("Failed to fetch settings")
                    pluginSelf.connectionFailed = true
                })
            )

            SettingsStore = settingsStore
            resolve(settingsStore)
        })
    }
}