/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
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
	ErrorBoundary,
	findModuleDetailsByExport,
} from '@steambrew/sdk';
import { ConditionsStore, ICondition, SliderConfig, ThemeColorOption, ThemeItem } from '../types';
import { settingsClasses } from '../utils/classes';
import { locale } from '../utils/localization-manager';
import { BBCodeParser, SettingsDialogSubHeader } from './SteamComponents';
import Styles from '../utils/styles';
import { backend } from '../utils/ffi';

interface ConditionalComponent {
	condition: string;
	value: ICondition;
	isLastItem: boolean;
}

interface ComponentInterface {
	conditionType: ConditionType;
	values: string[];
	conditionName: string;
	slider?: SliderConfig;
}

enum ConditionType {
	Dropdown,
	Toggle,
	Slider,
}

const SliderComponent = findModuleDetailsByExport(
	(m) => m?.prototype?.hasOwnProperty('BShouldTriggerHapticOnSnap') && m?.prototype?.hasOwnProperty('ComputeNormalizedValueForMousePosition'),
)[1];

interface ThemeEditorProps {
	theme: ThemeItem;
	isSidebar: boolean;
}

export class RenderThemeEditor extends React.Component<ThemeEditorProps> {
	GetConditionType = (condition: ICondition): ConditionType => {
		if (condition.slider) {
			return ConditionType.Slider;
		}
		if (condition.values && Object.keys(condition.values).every((element: string) => element === 'yes' || element === 'no')) {
			return ConditionType.Toggle;
		}
		return ConditionType.Dropdown;
	};

	UpdateLocalCondition = (conditionName: string, newData: string) => {
		const activeTheme: ThemeItem = this.props.theme as ThemeItem;

		return new Promise<boolean>((resolve) => {
			backend.themes.setThemeConfig(activeTheme.native, newData, conditionName).then((response) => {
				const success = response?.success ?? false;

				success && (pluginSelf.ConditionConfigHasChanged = true);
				resolve(success);
			});
		});
	};

	UpdateSliderCssVariable = (cssVariable: string, value: number, unit?: string) => {
		const cssValue = `${value}${unit ?? ''}`;
		for (const popup of g_PopupManager.GetPopups()) {
			const doc = popup.window.document;
			const styleId = `millennium-slider-${cssVariable}`;
			let el = doc.getElementById(styleId);
			if (!el) {
				el = doc.createElement('style');
				el.id = styleId;
				doc.head.appendChild(el);
			}
			el.innerText = `:root { ${cssVariable}: ${cssValue}; }`;
		}
	};

	RenderComponentInterface: React.FC<ComponentInterface> = ({ conditionType, values, conditionName, slider }) => {
		let store = pluginSelf?.conditionals?.[this.props.theme.native] as ConditionsStore;

		/** Dropdown items if given that the component is a dropdown */
		const items = values.map((value: string, index: number) => ({ label: value, data: 'componentId' + index }));

		const [isChecked, setIsChecked] = useState(store[conditionName] === 'yes');
		const [sliderValue, setSliderValue] = useState(Number(store[conditionName]) || slider?.min || 0);

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

			case ConditionType.Slider:
				return (
					<>
						<div className="MillenniumThemeSliderValue">
							{sliderValue}
							{slider?.unit ?? ''}
						</div>
						<SliderComponent
							min={slider?.min ?? 0}
							max={slider?.max ?? 100}
							step={slider?.step ?? 1}
							value={sliderValue}
							onChange={(newValue: number) => {
								setSliderValue(newValue);
								this.UpdateSliderCssVariable(slider?.cssVariable ?? '', newValue, slider?.unit);
							}}
							onChangeComplete={(newValue: number) => {
								this.UpdateLocalCondition(conditionName, String(newValue)).then((success) => {
									if (success) {
										store && (store[conditionName] = String(newValue));
									}
								});
							}}
						/>
					</>
				);
		}
	};

	UpdateCSSColors = (color: ThemeColorOption, newColor: string) => {
		for (const popup of g_PopupManager.GetPopups()) {
			const rootColors = popup.window.document.getElementById('RootColors');
			rootColors.innerHTML = rootColors.innerHTML.replace(new RegExp(`${color.color}:.*?;`, 'g'), `${color.color}: ${newColor};`);
		}
	};

	RenderComponent: React.FC<ConditionalComponent> = ({ condition, value, isLastItem }) => {
		const conditionType: ConditionType = this.GetConditionType(value);

		return (
			<Field
				label={condition}
				description={<BBCodeParser text={value?.description ?? 'No description yet.'} />}
				className={condition}
				key={condition}
				bottomSeparator={isLastItem ? 'none' : 'standard'}
				inlineWrap={conditionType === ConditionType.Slider ? 'shift-children-below' : undefined}
				verticalAlignment={conditionType === ConditionType.Slider ? 'none' : undefined}
			>
				<this.RenderComponentInterface conditionType={conditionType} conditionName={condition} values={Object.keys(value?.values ?? {})} slider={value?.slider} />
			</Field>
		);
	};

	RenderColorComponent: React.FC<{ color: ThemeColorOption; index: number }> = ({ color, index }) => {
		const debounceTimer = useRef<NodeJS.Timeout | null>(null);
		const [colorState, setColorState] = useState(color?.hex ?? '#000000');

		const saveColor = async (hexColor: string) => {
			const newColor = await backend.themes.setThemeColor(this.props.theme.native, color.color, hexColor, color.type);
			pluginSelf.RootColors = await backend.themes.getRootColors();
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
		const [themeColors, setThemeColors] = useState<ThemeColorOption[]>();

		useEffect(() => {
			backend.themes.colorOptions(this.props.theme.native).then(setThemeColors);
		}, []);

		return (
			<>
				{themeColors?.map?.((color, index) => (
					<this.RenderColorComponent color={color} index={index} />
				))}
			</>
		);
	};

	RenderSidebarEditor = ({ pages, activeTheme }: { pages: (SidebarNavigationPage | 'separator')[]; activeTheme: ThemeItem }) => {
		const tabItems = useMemo(
			() =>
				pages
					.filter((page): page is SidebarNavigationPage => typeof page !== 'string')
					.map((page) => ({
						label: page.title as string,
						data: page.content,
					})),
			[pages],
		);

		const [activeTab, setActiveTab] = useState(tabItems?.[0]?.label);

		const handleChange = useCallback((data: SingleDropdownOption) => {
			setActiveTab(data?.label as string);
		}, []);

		return (
			<div className="MillenniumDesktopSidebar_Editor" data-theme-name={activeTheme?.data?.name}>
				<div className="MillenniumDesktopSidebar_EditorHeader">
					<Dropdown
						contextMenuPositionOptions={{ bMatchWidth: true }}
						onChange={handleChange}
						rgOptions={tabItems}
						selectedOption={1}
						strDefaultLabel={tabItems?.[0]?.label}
					/>
				</div>
				<div className="MillenniumDesktopSidebar_EditorContent">
					<ErrorBoundary>{tabItems?.find?.((page) => page?.label === activeTab)?.data}</ErrorBoundary>
				</div>
			</div>
		);
	};

	RenderThemeEditor: React.FC = () => {
		const activeTheme = this.props.theme as ThemeItem;
		const isSidebar = this.props.isSidebar;

		const {
			data: { Conditions: themeConditions, RootColors, name },
		} = activeTheme;
		const entries = Object.entries(themeConditions ?? {});

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

		const createContentPage = (conditions: Record<string, ICondition>) => {
			const allEntries = Object.entries(conditions);
			const hasSections = allEntries.some(([, v]) => !!v.section);

			if (!hasSections) {
				return (
					<DialogBody className={Classes.SettingsDialogBodyFade}>
						{allEntries.map(([key, value], index) => (
							<this.RenderComponent condition={key} value={value} key={key} isLastItem={index === allEntries.length - 1} />
						))}
					</DialogBody>
				);
			}

			const sections: { name: string | undefined; items: [string, ICondition][] }[] = [];
			for (const entry of allEntries) {
				const sectionName = entry[1].section;
				const existing = sections.find((s) => s.name === sectionName);
				if (existing) {
					existing.items.push(entry);
				} else {
					sections.push({ name: sectionName, items: [entry] });
				}
			}

			return (
				<DialogBody className={Classes.SettingsDialogBodyFade}>
					{sections.map((section, sectionIndex) => (
						<React.Fragment key={section.name ?? '__default'}>
							{section.name && <SettingsDialogSubHeader>{section.name}</SettingsDialogSubHeader>}
							{section.items.map(([key, value], index) => (
								<this.RenderComponent
									condition={key}
									value={value}
									key={key}
									isLastItem={sectionIndex === sections.length - 1 && index === section.items.length - 1}
								/>
							))}
						</React.Fragment>
					))}
				</DialogBody>
			);
		};

		const createTabPages = () => {
			const pages = entries.reduce<{ title: string; conditions: Record<string, any>[] }[]>((acc, [name, patch]) => {
				const { tab } = patch;
				const condition = { [name]: patch };

				const existingTab = acc.find((p) => p.title === tab);
				if (existingTab) {
					existingTab.conditions.push(condition);
					return acc;
				}

				acc.push({ title: tab ?? '', conditions: [condition] });
				return acc;
			}, []);

			return pages.map(({ title, conditions }) => ({
				title,
				content: createContentPage(conditions.reduce((a, b) => ({ ...a, ...b }), {})),
			}));
		};

		const tabPages = createTabPages();
		const foundDefaultPage = tabPages.find((p) => !p.title);
		const defaultPage: SidebarNavigationPage = {
			title: locale.customThemeSettingsConfig,
			content: foundDefaultPage?.content ?? <></>,
		};
		const finalTabs: SidebarNavigationPage[] = hasTabs ? (tabPages.filter((p) => p !== foundDefaultPage) as SidebarNavigationPage[]) : [defaultPage];
		const className = [settingsClasses.SettingsModal, settingsClasses.DesktopPopup, 'MillenniumSettings'].join(' ');
		const pages: (SidebarNavigationPage | 'separator')[] = [...finalTabs, ...(hasColors ? ['separator' as const, createColorPage()] : [])];
		const title = name ?? activeTheme.native;

		const hidePageListStyles = !hasTabs && !hasColors ? <style>{`.PageListColumn { display: none !important; }`}</style> : null;

		if (isSidebar) {
			return <this.RenderSidebarEditor pages={pages} activeTheme={activeTheme} />;
		}

		return (
			<ModalPosition>
				<Styles />
				{hidePageListStyles}
				<SidebarNavigation className={className} pages={pages} title={title} />
			</ModalPosition>
		);
	};

	render() {
		return <this.RenderThemeEditor />;
	}
}
