import { DefineTab, IconsModule, MillenniumSettingTabs } from '@steambrew/client';
import { BasicExampleSettings } from './BasicExampleSettings';
import { ComplexSettings } from './ComplexSettings';

const puzzleIcon = <svg fill="currentColor" viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
    <path d="M2,20.889V16.444H4.222a2.224,2.224,0,0,0,2.192-2.595A2.305,2.305,0,0,0,4.1,12H2V7.556A1.111,1.111,0,0,1,3.111,6.444H7.556V4.222a2.224,2.224,0,0,1,2.6-2.192A2.305,2.305,0,0,1,12,4.341v2.1h4.444a1.112,1.112,0,0,1,1.112,1.112V12h2.1a2.305,2.305,0,0,1,2.311,1.849,2.224,2.224,0,0,1-2.192,2.595H17.556v4.445A1.111,1.111,0,0,1,16.444,22H13.111V19.778a2.224,2.224,0,0,0-2.6-2.192A2.305,2.305,0,0,0,8.667,19.9V22H3.111A1.11,1.11,0,0,1,2,20.889Z"/>
</svg>;

export class ExampleSettingsWithTabs extends MillenniumSettingTabs<ExampleSettingsWithTabs> {
    @DefineTab({title: 'Basic Settings', icon: IconsModule.Settings})
    public BasicSettings = new BasicExampleSettings();

    @DefineTab({title: 'Playground', icon: puzzleIcon})
    public Playground = new ComplexSettings();
}
