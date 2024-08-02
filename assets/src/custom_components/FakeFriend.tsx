import { Classes } from "@millennium/ui";
import React, { ReactNode } from "react";

interface FakeFriendProps {
	eStatus?: string;
	strAvatarURL?: string;
	strGameName?: ReactNode;
	strPlayerName?: ReactNode;
	onClick?: () => void;
}

/**
 * @todo Get & use the webpack module for this
 */
export const FakeFriend: React.FC<FakeFriendProps> = ({
	eStatus,
	strAvatarURL,
	strGameName,
	strPlayerName,
	onClick,
}) => (
	<div className={`${Classes.FakeFriend} ${eStatus}`} onClick={onClick}>
		<div
			className={`${Classes.avatarHolder} avatarHolder no-drag Medium ${eStatus}`}
		>
			<div className={`${Classes.avatarStatus} avatarStatus right`} />
			<img
				src={strAvatarURL}
				className={`${Classes.avatar} avatar`}
				draggable="false"
			/>
		</div>
		<div className={`${eStatus} ${Classes.noContextMenu} ${Classes.twoLine}`}>
			<div className={Classes.statusAndName}>
				<div className={Classes.playerName}>{strPlayerName}</div>
			</div>
			<div className={Classes.richPresenceContainer} style={{ width: "100%" }}>
				<div
					className={`${Classes.gameName} ${Classes.richPresenceLabel} no-drag`}
				>
					{strGameName}
				</div>
			</div>
		</div>
	</div>
);
