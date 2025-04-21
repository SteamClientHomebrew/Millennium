import React, { useState } from 'react';
import { Dropdown, SingleDropdownOption, Toggle, Field, findClassModule, TextField } from '@steambrew/client';
import Markdown from 'markdown-to-jsx';

interface ComponentInterface {
	object: string;
	component: any;
	type: ComponentType;
	pluginName: string;
}

interface PluginEditorProps {
	plugin: any;
}

enum ComponentType {
	DropDown,
	CheckBox,
	NumberTextInput,
	StringTextInput,
	FloatTextInput,
}

const RenderComponentType: React.FC<ComponentInterface> = ({ pluginName, object, component, type }) => {
	const settings = window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[pluginName].settingsStore;
	const items = component?.options?.map((value: string, index: number) => ({ label: value, data: 'componentId' + index }));

	const [checked, setChecked] = useState<boolean>(component.value);
	const [textValue, setTextValue] = useState<string | number>(component.value);

	const OnDropDownChange = (option: SingleDropdownOption) => {
		settings[object] = option.label;
	};

	const OnCheckBoxChange = (checked: boolean) => {
		settings[object] = checked;
		setChecked(checked);
	};

	const OnNumberTextInputChange = (event: React.ChangeEvent<HTMLInputElement>) => {
		console.log('Text input changed', event.target.value);

		try {
			settings[object] = parseInt(event.target.value);
			setTextValue(settings[object]);
		} catch (e) {
			console.error('Error parsing number', e);
		}
	};

	switch (type) {
		case ComponentType.DropDown:
			return (
				<Dropdown
					key={pluginName + object}
					contextMenuPositionOptions={{ bMatchWidth: false }}
					onChange={OnDropDownChange}
					rgOptions={items}
					selectedOption={1}
					strDefaultLabel={component.value}
				/>
			);

		case ComponentType.CheckBox:
			return <Toggle key={pluginName + object} value={checked} onChange={OnCheckBoxChange} />;

		case ComponentType.NumberTextInput:
			return (
				<TextField
					key={pluginName + object}
					value={textValue as string}
					onChange={OnNumberTextInputChange}
					className="DialogInput DialogInputPlaceholder DialogTextInputBase Focusable"
				/>
			);

		default:
			return <></>;
	}
};

const SettingsModalClass = (findClassModule((m) => m.SettingsModal) as any).SettingsModal;

const popupStyles = `
/** Override the default width of the popup */
.DialogContent._DialogLayout.GenericConfirmDialog._DialogCenterVertically {
	width: 635px;
}
/** Hide the cancel button as it doesn't apply here */
button.DialogButton._DialogLayout.Secondary.Focusable {
	display: none;
}
.${SettingsModalClass} {
	min-width: unset;
}
`;

const RenderComponents: React.FC<PluginEditorProps> = ({ plugin }) => {
	const settings = window.MILLENNIUM_PLUGIN_SETTINGS_STORE?.[plugin?.data?.name].settingsStore.__raw_get_internals__;

	const components = Object.keys(settings).map((name, index) => {
		const value = settings[name];

		return (
			<Field
				label={value.name}
				description={<Markdown>{value?.desc ?? 'No description yet.'}</Markdown>}
				key={index}
				bottomSeparator={Object.keys(settings).length - 1 === index ? 'none' : 'standard'}
				focusable
			>
				<RenderComponentType pluginName={plugin?.data?.name} object={name} component={value} type={ComponentType[value.type as keyof typeof ComponentType]} />
			</Field>
		);
	});

	return (
		<>
			<div className={`${SettingsModalClass} MillenniumSettings Panel`}>
				<style>{popupStyles}</style>
				{components}
			</div>
		</>
	);
};

export { RenderComponents };
