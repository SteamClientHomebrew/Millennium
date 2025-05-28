import { BindPluginSettings } from '@steambrew/webkit';

type CheckBox = true | false;

type EnumerateInternal<N extends number, Acc extends number[] = []> = Acc['length'] extends N ? Acc[number] : EnumerateInternal<N, [...Acc, Acc['length']]>;

type Enumerate<Min extends number, Max extends number> = Exclude<EnumerateInternal<Max>, EnumerateInternal<Min>> | Max;

type NumberTextInput<Min extends number, Max extends number> = Min | Enumerate<Min, Max>;

type DropDown<T extends readonly any[]> = T[number];

interface SettingsProps {
	doFrontEndCall: CheckBox;
	overrideWebkitDocument: CheckBox;
	numberTextInput: NumberTextInput<1, 100>;
	frontEndMessage: DropDown<['hello', 'hi', 'hello again', false, 69]>;
}

export let PluginSettings: SettingsProps = BindPluginSettings();
