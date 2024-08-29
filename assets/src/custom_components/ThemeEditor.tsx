import React, { useEffect, useState } from "react"
import {
    Classes,
    DialogBody,
    DialogBodyText,
    DialogButton,
    DialogControlsSection,
    DialogHeader,
    Dropdown,
    Millennium,
    SidebarNavigation,
    type SidebarNavigationPage,
    SingleDropdownOption,
    Toggle,
    pluginSelf,
  } from "millennium-lib";
import { BBCodeParser } from "../components/BBCodeParser";
import { Field } from "../custom_components/Field";
import { Conditions, ConditionsStore, ICondition, ThemeItem } from "../types"
import { settingsClasses } from "../classes"
import { locale } from "../locales"
import { SettingsDialogSubHeader } from "../components/SettingsDialogSubHeader";

interface ConditionalComponent {
    condition: string,
    value: ICondition,
    store: ConditionsStore
}

interface ComponentInterface {
    conditionType: ConditionType, 
    values: string[], 
    conditionName: string,
    store: ConditionsStore
}

enum ConditionType {
    Dropdown,
    Toggle
}

enum ColorTypes {
    RawRGB = 1,
    RGB = 2,
    RawRGBA = 3,
    RGBA = 4,
    Hex = 5,
    Unknown = 6
}

interface ColorProps {
    name?: string,
    description?: string,
    color: string,
    type: ColorTypes,
    hex: string,
    defaultColor: string
}

export class RenderThemeEditor extends React.Component {

    GetConditionType = (value: any): ConditionType => {

        if (Object.keys(value).every((element: string) => element === 'yes' || element === 'no')) {
            return ConditionType.Toggle
        }
        else {
            return ConditionType.Dropdown
        }
    }
    
    UpdateLocalCondition = (conditionName: string, newData: string) => {
        const activeTheme: ThemeItem = pluginSelf.activeTheme as ThemeItem
    
        return new Promise<boolean>((resolve) => {
            Millennium.callServerMethod("cfg.change_condition", {
                theme: activeTheme.native, newData: newData, condition: conditionName
            })
            .then((result: any) => {
                pluginSelf.connectionFailed = false
                return result
            })
            .then((response: any) => {

                const success = JSON.parse(response)?.success as boolean ?? false

                success && (pluginSelf.ConditionConfigHasChanged = true)
                resolve(success)
            })
        })
    }
    
    RenderComponentInterface: React.FC<ComponentInterface> = ({ conditionType, values, store, conditionName }) => {
    
        /** Dropdown items if given that the component is a dropdown */
        const items = values.map((value: string, index: number) => ({ label: value, data: "componentId" + index }))
    
        /** Checked status if given that the component is a toggle */
        const [checked, setChecked] = useState(store?.[conditionName] == "yes" ? true : false)
        // const [isHovered, setIsHovered] = useState({state: false, target: null});
    
        const onCheckChange = (enabled: boolean) => {
    
            this.UpdateLocalCondition(conditionName, enabled ? "yes" : "no").then((success) => {
                success && setChecked(enabled)
            })
        }
    
        const onDropdownChange = (data: SingleDropdownOption) => {
            this.UpdateLocalCondition(conditionName, data.label as string)
        }

        switch (conditionType) {
            case ConditionType.Dropdown: 
                // @ts-ignore
                return <Dropdown contextMenuPositionOptions={{bMatchWidth: false}} onChange={onDropdownChange} rgOptions={items} selectedOption={1} strDefaultLabel={store[conditionName]}/>
    
            case ConditionType.Toggle:
                return <Toggle value={checked} onChange={onCheckChange} />
        }
    }
    
    RenderComponent: React.FC<ConditionalComponent> = ({condition, value, store}) => {
    
        const conditionType: ConditionType = this.GetConditionType(value.values)

        return (
            <Field
                label={condition}
                description=<BBCodeParser text={value?.description ?? "No description yet."} />
            >
                <this.RenderComponentInterface conditionType={conditionType} store={store} conditionName={condition} values={Object.keys(value?.values)} />
            </Field>
        )
    }
    
    RenderColorComponent: React.FC<{color: ColorProps, index: number}> = ({color, index}) => {

        const [colorState, setColorState] = useState(color?.hex ?? "#000000");
        (window as any).lastColorChangeTime = performance.now();

        const UpdateColor = (hexColor: string) => {
            if (performance.now() - (window as any).lastColorChangeTime < 5) { 
                return;
            }

            setColorState(hexColor)

            Millennium.callServerMethod("cfg.change_color", { color_name: color.color, new_color: hexColor, type: color.type })
            .then((result: any) => {
                // @ts-ignore
                g_PopupManager.m_mapPopups.data_.forEach((element: any) => {
                    var rootColors = element.value_.m_popup.window.document.getElementById("RootColors");
                    rootColors.innerHTML = rootColors.innerHTML.replace(new RegExp(`${color.color}:.*?;`, 'g'), `${color.color}: ${result};`)
                })
            })
        }

        const ResetColor = () => {
            UpdateColor(color.defaultColor)
        }

        return (
            <Field
                key={index}
                label={color?.name ?? color?.color}
                description=<BBCodeParser text={color?.description ?? "No description yet."} />
            >
                {colorState != color.defaultColor && <DialogButton className={settingsClasses.SettingsDialogButton} onClick={ResetColor}>Reset</DialogButton>}
                <input type="color" className="colorPicker" name="colorPicker" value={colorState} onChange={(event) => UpdateColor(event.target.value)}/>
            </Field>
        )
    }

    RenderColorsOpts: React.FC = () => {
        const activeTheme: ThemeItem = pluginSelf.activeTheme as ThemeItem
        const [themeColors, setThemeColors] = useState<ColorProps[]>()

        useEffect(() => {
            if (activeTheme?.data?.RootColors) {
                Millennium.callServerMethod("cfg.get_color_opts")
                .then((result: any) => {
                    console.log(JSON.parse(result) as ColorProps[])
                    setThemeColors(JSON.parse(result) as ColorProps[])
                })
            }
        }, [])

        return themeColors && <DialogControlsSection>
            <SettingsDialogSubHeader>{locale.customThemeSettingsColorsHeader}</SettingsDialogSubHeader>
            <DialogBodyText className='_3fPiC9QRyT5oJ6xePCVYz8'>{locale.customThemeSettingsColorsDescription}</DialogBodyText>

            {themeColors?.map((color: any, index: number) => <this.RenderColorComponent color={color} index={index}/>)}
        </DialogControlsSection>      
    }

    render() {
        const activeTheme: ThemeItem = pluginSelf.activeTheme as ThemeItem
    
        const themeConditions: Conditions = activeTheme.data.Conditions
        const savedConditions = pluginSelf?.conditionals?.[activeTheme.native] as ConditionsStore

		const pages: SidebarNavigationPage[] = Object.entries(themeConditions)
			.reduce<{ title: string; conditions: Conditions[] }[]>((vec, entry) => {
				const [name, patch] = entry;
				const { tab } = patch;
				const condition = { [name]: patch };
				const page = {
					title: tab,
					conditions: [condition],
				};

				const foundTab = vec.find((e) => e.title === tab);
				if (foundTab) {
					foundTab.conditions.push(condition);
					return vec;
				}

				vec.push(page);
				return vec;
			}, [])
			.map(({ title, conditions }) => ({
				title,
				content: (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						{Object.entries(
							conditions.reduce((a, b) => Object.assign(a, b)),
						).map(([key, value]) => (
							<this.RenderComponent
								condition={key}
								store={savedConditions}
								value={value}
							/>
						))}
					</DialogBody>
				),
			}));
		const title = `Editing ${activeTheme?.data?.name ?? activeTheme.native}`;

		return <SidebarNavigation pages={pages} title={title} />; 
    }
}