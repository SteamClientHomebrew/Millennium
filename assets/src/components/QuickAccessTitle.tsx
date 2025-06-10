import { DialogButton, Focusable, Navigation, staticClasses } from '@steambrew/client';
import { useDesktopMenu } from '../quick-access/DesktopMenuContext';
import { CSSProperties } from 'react';
import { BsGearFill } from 'react-icons/bs';
import { FaArrowLeft } from 'react-icons/fa';
import { getPluginView } from '../utils/globals';

const titleStyles: CSSProperties & any = {
	display: 'flex',
	paddingTop: '65px',
	paddingRight: '16px',
	position: 'sticky',
	top: '0px',
};

export const TitleView = () => {
	const { setDesktopMenuOpen, activePlugin, setActivePlugin } = useDesktopMenu();

	const onSettingsClick = () => {
		Navigation.Navigate('/millennium/settings');
		setDesktopMenuOpen(false);
	};

	const buttonStyles = {
		height: '28px',
		width: '40px',
		minWidth: 0,
		padding: '',
		display: 'flex',
		alignItems: 'center',
		justifyContent: 'center',
		boxShadow: '-4px -35px -1px 19px #0e141b',
	};

	if (!activePlugin) {
		return (
			<Focusable style={titleStyles} className={staticClasses.Title}>
				<div style={{ marginRight: 'auto', flex: 0.9 }}>Millennium</div>
				<DialogButton style={buttonStyles} onClick={onSettingsClick}>
					<BsGearFill style={{ marginTop: '-4px', display: 'block' }} />
				</DialogButton>
			</Focusable>
		);
	}

	return (
		<Focusable style={titleStyles} className={staticClasses.Title}>
			<DialogButton style={buttonStyles} onClick={setActivePlugin.bind(null, null)}>
				<FaArrowLeft style={{ marginTop: '-4px', display: 'block' }} />
			</DialogButton>
			{getPluginView(activePlugin?.data?.name)?.titleView || <div style={{ marginRight: 'auto', flex: 0.9 }}>{activePlugin?.data?.common_name}</div>}
		</Focusable>
	);
};
