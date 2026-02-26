import { FC, ReactNode, createElement, useEffect, useState } from 'react';

import { fakeRenderComponent, findInReactTree, sleep } from '../utils';
import { Export, findModuleByExport } from '../webpack';
import { FooterLegendProps } from './FooterLegend';
import { SteamSpinner } from './SteamSpinner';

/**
 * Individual tab objects for the Tabs component
 *
 * `id` ID of this tab, can be used with activeTab to auto-focus a given tab
 * `title` Title shown in the header bar
 * `renderTabAddon` Return a {@link ReactNode} to render it next to the tab title, i.e. the counts for each tab on the Media page
 * `content` Content of the tab
 * `footer` Sets up button handlers and labels
 */
export interface Tab {
	id: string;
	title: string;
	renderTabAddon?: () => ReactNode;
	content: ReactNode;
	footer?: FooterLegendProps;
}

/**
 * Props for the {@link Tabs}
 *
 * `tabs` array of {@link Tab}
 * `activeTab` tab currently active, needs to be one of the tabs {@link Tab.id}, must be set using a `useState` in the `onShowTab` handler
 * `onShowTab` Called when the active tab should change, needs to set `activeTab`. See example.
 * `autoFocusContents` Whether to automatically focus the tab contents or not.
 * `footer` Sets up button handlers and labels
 *
 * @example
 * const Component: FC = () => {
 * const [currentTab, setCurrentTab] = useState<string>("Tab1");
 *
 * return (
 *   <Tabs
 *     title="Theme Manager"
 *     activeTab={currentTabRoute}
 *     onShowTab={(tabID: string) => {
 *        setCurrentTabRoute(tabID);
 *     }}
 *     tabs={[
 *       {
 *         title: "Tab 1",
 *         content: <Tab1Component />,
 *         id: "Tab1",
 *       },
 *       {
 *         title: "Tab 2",
 *         content: <Tab2Component />,
 *         id: "Tab2",
 *       },
 *     ]}
 *   />
 *  );
 * };
 */
export interface TabsProps {
	tabs: Tab[];
	activeTab: string;
	onShowTab: (tab: string) => void;
	autoFocusContents?: boolean;
}

let tabsComponent: any;

const getTabs = async () => {
	if (tabsComponent) return tabsComponent;
	// @ts-ignore
	while (!window?.DeckyPluginLoader?.routerHook?.routes) {
		console.debug('[Millennium:Tabs]: Waiting for Millennium router...');
		await sleep(500);
	}
	return (tabsComponent = fakeRenderComponent(
		() => {
			return findInReactTree(
				findInReactTree(
					// @ts-ignore
					window.DeckyPluginLoader.routerHook.routes.find((x: any) => x.props.path == '/library/app/:appid/achievements').props.children.type(),
					(x) => x?.props?.scrollTabsTop,
				).type({ appid: 1 }),
				(x) => x?.props?.tabs,
			).type;
		},
		{
			useRef: () => ({ current: { reaction: { track: () => {} } } }),
			useContext: () => ({ match: { params: { appid: 1 } } }),
			useMemo: () => ({ data: {} }),
		},
	));
};

let oldTabs: any;

try {
	const oldTabsModule = findModuleByExport((e: Export) => e.Unbleed);
	if (oldTabsModule) oldTabs = Object.values(oldTabsModule).find((x: any) => x?.type?.toString()?.includes('((function(){'));
} catch (e) {
	console.error('Error finding oldTabs:', e);
}

/**
 * Tabs component as used in the library and media tabs. See {@link TabsProps}.
 */
export const Tabs = (oldTabs ||
	((props: TabsProps) => {
		const found = tabsComponent;
		const [tc, setTC] = useState<FC<TabsProps>>(found);
		useEffect(() => {
			if (found) return;
			(async () => {
				console.debug('[DFL:Tabs]: Finding component...');
				const t = await getTabs();
				console.debug('[DFL:Tabs]: Found!');
				setTC(t);
			})();
		}, []);
		console.log('tc', tc);
		return tc ? createElement(tc, props) : <SteamSpinner />;
	})) as FC<TabsProps>;
