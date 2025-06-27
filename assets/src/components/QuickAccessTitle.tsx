import { Focusable, joinClassNames, Navigation, quickAccessMenuClasses } from '@steambrew/client';
import { useDesktopMenu } from '../quick-access/DesktopMenuContext';
import { BsGearFill } from 'react-icons/bs';
import { FaArrowLeft } from 'react-icons/fa';
import { getPluginView } from '../utils/globals';
import { IconButton } from './IconButton';

export const TitleView = () => {
	const { closeMenu, activePlugin, setActivePlugin } = useDesktopMenu();

	const onSettingsClick = () => {
		Navigation.Navigate('/millennium/settings');
		closeMenu();
	};

	if (!activePlugin) {
		return (
			<Focusable className={joinClassNames('MillenniumDesktopSidebar_Title', quickAccessMenuClasses.Title)}>
				<div>Millennium</div>
				<IconButton onClick={onSettingsClick}>
					<BsGearFill />
				</IconButton>
			</Focusable>
		);
	}

	return (
		<Focusable className={joinClassNames('MillenniumDesktopSidebar_Title', quickAccessMenuClasses.Title)}>
			<IconButton onClick={setActivePlugin.bind(null, null)}>
				<FaArrowLeft />
			</IconButton>
			{getPluginView(activePlugin?.data?.name)?.titleView || <div>{activePlugin?.data?.common_name}</div>}
		</Focusable>
	);
};
