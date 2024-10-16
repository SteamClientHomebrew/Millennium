import { Millennium } from "millennium-lib";
import { ConditionalControlFlowType, Theme, ThemeItem } from "./types";
import { constructThemePath } from "./patcher/Dispatch";

interface WebkitItem {
    matchString: string,
    targetPath: string,
    fileType: ConditionalControlFlowType
}

const ParseMatchAndTarget = (theme: Theme): WebkitItem[] => {

    let webkitItems: WebkitItem[] = []

    // Add condition keys into the webkitItems array
    for (const item in theme?.Conditions) {
        const condition = theme.Conditions[item]

        for (const value in condition?.values) {
            const controlFlow = condition.values[value]


            for (const control in controlFlow?.TargetCss?.affects) {
                const matchString = controlFlow.TargetCss.affects[control]
                const targetPath = controlFlow.TargetCss.src

                webkitItems.push({ matchString, targetPath, fileType: ConditionalControlFlowType.TargetCss })
            }

            for (const control in controlFlow?.TargetJs?.affects) {
                const matchString = controlFlow.TargetJs.affects[control]
                const targetPath = controlFlow.TargetJs.src

                webkitItems.push({ matchString, targetPath, fileType: ConditionalControlFlowType.TargetJs })
            }
        }
    }

    // Add patch keys into the webkitItems array
    for (const item in theme?.Patches) {
        const patch = theme.Patches[item]

        if (patch.TargetCss) {
            if (Array.isArray(patch.TargetCss)) {
                for (const target in patch.TargetCss) {
                    webkitItems.push({ matchString: patch.MatchRegexString, targetPath: patch.TargetCss[target], fileType: ConditionalControlFlowType.TargetCss })
                }
            }
            else {
                webkitItems.push({ matchString: patch.MatchRegexString, targetPath: patch.TargetCss, fileType: ConditionalControlFlowType.TargetCss })
            }
        }

        if (patch.TargetJs) { 
            if (Array.isArray(patch.TargetJs)) {
                for (const target in patch.TargetJs) {
                    webkitItems.push({ matchString: patch.MatchRegexString, targetPath: patch.TargetJs[target], fileType: ConditionalControlFlowType.TargetJs })
                }
            }
            else {
                webkitItems.push({ matchString: patch.MatchRegexString, targetPath: patch.TargetJs, fileType: ConditionalControlFlowType.TargetJs })
            }
        }
    }

    // Filter out duplicates
    webkitItems = webkitItems.filter((item, index, self) =>
        index === self.findIndex((t) => (
            t.matchString === item.matchString && t.targetPath === item.targetPath
        ))
    )

    return webkitItems
}

const CreateWebkitScript = (themeData: ThemeItem) => {
    console.log(themeData)

    let script = ``;
    const webkitItems: WebkitItem[] = ParseMatchAndTarget(themeData?.data)

    for (const item in webkitItems) {
        const webkitItem = webkitItems[item]

        const cssPath = constructThemePath(themeData?.native, webkitItem.targetPath)
        const jsPath = ["https://s.ytimg.com/millennium-virtual", "skins", themeData?.native, webkitItem.targetPath].join('/')

        script += `
            if (RegExp("${webkitItem.matchString}").test(window.location.href)) {
                console.log("Matched: ${webkitItem.matchString}, injecting ${webkitItem.fileType === ConditionalControlFlowType.TargetCss ? cssPath : jsPath}");
                ${
                    webkitItem.fileType === ConditionalControlFlowType.TargetCss
                        ? `document.head.appendChild(Object.assign(document.createElement('link'), { rel: 'stylesheet', href: '${cssPath}' }))`
                        : `document.head.appendChild(Object.assign(document.createElement('script'), { src: '${jsPath}' }))`
                }
            }\n
        `
    }

    console.log(script)
    Millennium.callServerMethod("inject_webkit_shim", { shim_script: 
        `(() => { ${script} })()`
    })
}

export { CreateWebkitScript }