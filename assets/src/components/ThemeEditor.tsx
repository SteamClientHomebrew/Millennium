/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import React, { useEffect, useRef, useState } from 'react';
import {
	Classes,
	DialogBody,
	DialogButton,
	Dropdown,
	ModalPosition,
	SidebarNavigation,
	type SidebarNavigationPage,
	SingleDropdownOption,
	Toggle,
	pluginSelf,
	Field,
} from '@steambrew/client';
import { Conditions, ConditionsStore, ICondition, ThemeItem } from '../types';
import { settingsClasses } from '../utils/classes';
import { locale } from '../../locales';
import { BBCodeParser } from './SteamComponents';
import Styles from '../utils/styles';
import { PyChangeColor, PyChangeCondition, PyGetRootColors, PyGetThemeColorOptions } from '../utils/ffi';

interface ConditionalComponent {
	condition: string;
	value: ICondition;
	isLastItem: boolean;
}

interface ComponentInterface {
	conditionType: ConditionType;
	values: string[];
	conditionName: string;
}

enum ConditionType {
	Dropdown,
	Toggle,
}

enum ColorTypes {
	RawRGB = 1,
	RGB = 2,
	RawRGBA = 3,
	RGBA = 4,
	Hex = 5,
	Unknown = 6,
}

interface ColorProps {
	name?: string;
	description?: string;
	color: string;
	type: ColorTypes;
	hex: string;
	defaultColor: string;
}

interface ThemeEditorProps {
	theme: ThemeItem;
}

export class RenderThemeEditor extends React.Component<ThemeEditorProps> {
	GetConditionType = (value: any): ConditionType => {
		if (Object.keys(value).every((element: string) => element === 'yes' || element === 'no')) {
			return ConditionType.Toggle;
		} else {
			return ConditionType.Dropdown;
		}
	};

	UpdateLocalCondition = (conditionName: string, newData: string) => {
		const activeTheme: ThemeItem = this.props.theme as ThemeItem;

		return new Promise<boolean>((resolve) => {
			PyChangeCondition({
				theme: activeTheme.native,
				newData: newData,
				condition: conditionName,
			}).then((response: any) => {
				const success = (JSON.parse(response)?.success as boolean) ?? false;

				success && (pluginSelf.ConditionConfigHasChanged = true);
				resolve(success);
			});
		});
	};

	RenderComponentInterface: React.FC<ComponentInterface> = ({ conditionType, values, conditionName }) => {
		let store = pluginSelf?.conditionals?.[this.props.theme.native] as ConditionsStore;
		console.log(store, this.props.theme);

		/** Dropdown items if given that the component is a dropdown */
		const items = values.map((value: string, index: number) => ({ label: value, data: 'componentId' + index }));

		const [isChecked, setIsChecked] = useState(store[conditionName] === 'yes');

		useEffect(() => {
			setIsChecked(store[conditionName] === 'yes');
		}, [store[conditionName]]);

		const onCheckChange = (enabled: boolean) => {
			this.UpdateLocalCondition(conditionName, enabled ? 'yes' : 'no').then((success) => {
				if (success) {
					store && (store[conditionName] = enabled ? 'yes' : 'no');
					setIsChecked(enabled);
				}
			});
		};

		const onDropdownChange = (data: SingleDropdownOption) => {
			this.UpdateLocalCondition(conditionName, data.label as string).then((success) => {
				success && store && (store[conditionName] = data.label as string);
			});
		};

		switch (conditionType) {
			case ConditionType.Dropdown:
				// @ts-ignore
				return (
					<Dropdown
						contextMenuPositionOptions={{ bMatchWidth: false }}
						onChange={onDropdownChange}
						rgOptions={items}
						selectedOption={1}
						strDefaultLabel={store[conditionName]}
					/>
				);

			case ConditionType.Toggle:
				return <Toggle key={conditionName} value={isChecked} onChange={onCheckChange} navRef={conditionName} />;
		}
	};

	UpdateCSSColors = (color: ColorProps, newColor: string) => {
		for (const popup of g_PopupManager.GetPopups()) {
			const rootColors = popup.window.document.getElementById('RootColors');
			rootColors.innerHTML = rootColors.innerHTML.replace(new RegExp(`${color.color}:.*?;`, 'g'), `${color.color}: ${newColor};`);
		}
	};

	RenderComponent: React.FC<ConditionalComponent> = ({ condition, value, isLastItem }) => {
		const conditionType: ConditionType = this.GetConditionType(value.values);

		return (
			<Field
				label={condition}
				description={<BBCodeParser text={value?.description ?? 'No description yet.'} />}
				className={condition}
				key={condition}
				bottomSeparator={isLastItem ? 'none' : 'standard'}
			>
				<this.RenderComponentInterface conditionType={conditionType} conditionName={condition} values={Object.keys(value?.values)} />
			</Field>
		);
	};

	RenderColorComponent: React.FC<{ color: ColorProps; index: number }> = ({ color, index }) => {
		const debounceTimer = useRef<NodeJS.Timeout | null>(null);
		const [colorState, setColorState] = useState(color?.hex ?? '#000000');

		const saveColor = async (hexColor: string) => {
			const newColor = await PyChangeColor({ theme: this.props.theme.native, color_name: color.color, new_color: hexColor, color_type: color.type });
			pluginSelf.RootColors = await PyGetRootColors();
			return newColor;
		};

		const debounceColorUpdate = (hexColor: string) => {
			setColorState(hexColor);

			if (debounceTimer.current) {
				clearTimeout(debounceTimer.current);
			}

			debounceTimer.current = setTimeout(async () => {
				const newColor = await saveColor(hexColor);
				this.UpdateCSSColors(color, newColor);
			}, 300);
		};

		const resetColor = async () => {
			setColorState(color.defaultColor);
			const defaultColor = await saveColor(color.defaultColor);
			this.UpdateCSSColors(color, defaultColor);
		};

		return (
			<Field key={index} label={color?.name ?? color?.color} description={<BBCodeParser text={color?.description ?? 'No description yet.'} />}>
				{colorState != color.defaultColor && (
					<DialogButton className={settingsClasses.SettingsDialogButton} onClick={resetColor}>
						Reset
					</DialogButton>
				)}
				<input type="color" className="MillenniumColorPicker" value={colorState} onChange={(event) => debounceColorUpdate(event.target.value)} />
			</Field>
		);
	};

	RenderColorsOpts: React.FC = () => {
		const [themeColors, setThemeColors] = useState<ColorProps[]>();

		useEffect(() => {
			PyGetThemeColorOptions({ theme_name: this.props.theme.native }).then((result: any) => {
				setThemeColors(JSON.parse(result) as ColorProps[]);
			});
		}, []);

		return (
			<>
				{themeColors?.map?.((color, index) => (
					<this.RenderColorComponent color={color} index={index} />
				))}
			</>
		);
	};

	RenderThemeEditor: React.FC = () => {
		const activeTheme = this.props.theme as ThemeItem;
		const {
			data: { Conditions: themeConditions, RootColors, name },
		} = activeTheme;
		const entries = Object.entries(themeConditions);

		const hasColors = !!RootColors;
		const hasTabs = entries.some(([, { tab }]) => !!tab);

		const createColorPage = (): SidebarNavigationPage => ({
			title: locale.customThemeSettingsColors,
			content: (
				<DialogBody className={Classes.SettingsDialogBodyFade}>
					<this.RenderColorsOpts />
				</DialogBody>
			),
		});

		const createContentPage = (conditions: Record<string, any>) => (
			<DialogBody className={Classes.SettingsDialogBodyFade}>
				{Object.entries(conditions).map(([key, value], index) => (
					<this.RenderComponent condition={key} value={value} key={key} isLastItem={index === Object.keys(conditions).length - 1} />
				))}
			</DialogBody>
		);

		const createTabPages = () => {
			const pages = entries.reduce<{ title: string; conditions: Record<string, any>[] }[]>((acc, [name, patch]) => {
				const { tab } = patch;
				const condition = { [name]: patch };

				const existingTab = acc.find((p) => p.title === tab);
				if (existingTab) {
					existingTab.conditions.push(condition);
					return acc;
				}

				acc.push({ title: tab, conditions: [condition] });
				return acc;
			}, []);

			return pages.map(({ title, conditions }) => ({
				title,
				content: createContentPage(conditions.reduce((a, b) => ({ ...a, ...b }), {})),
			}));
		};

		const tabPages = createTabPages();
		const defaultPage = { ...tabPages.find((p) => !p.title), title: locale.customThemeSettingsConfig };
		const finalTabs = hasTabs ? tabPages.filter((p) => p !== defaultPage) : [defaultPage];
		const className = [settingsClasses.SettingsModal, settingsClasses.DesktopPopup, 'MillenniumSettings'].join(' ');
		const pages = [...finalTabs, ...(hasColors ? ['separator', createColorPage()] : [])];
		const title = name ?? activeTheme.native;

		const hidePageListStyles = !hasTabs && !hasColors ? <style>{`.PageListColumn { display: none !important; }`}</style> : null;

		return (
			<ModalPosition>
				<Styles />
				{hidePageListStyles}
				{/* @ts-ignore: className hasn't been added to DFL yet */}
				<SidebarNavigation className={className} pages={pages} title={title} />
			</ModalPosition>
		);
	};

	render() {
		return <this.RenderThemeEditor />;
	}
}
