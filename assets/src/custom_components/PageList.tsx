import React, { ReactNode } from "react";
import { Classes } from "@millennium/ui";
import { pagedSettingsClasses } from "../classes";

const active: string = pagedSettingsClasses.Active;

interface ItemProps {
	bSelected: boolean;
	icon?: ReactNode;
	title?: ReactNode;
	onClick?: () => void;
}

export const Item: React.FC<ItemProps> = ({
	bSelected,
	icon,
	title,
	onClick,
}) => (
	<div
		className={`MillenniumTab ${Classes.PagedSettingsDialog_PageListItem} ${bSelected && active}`}
		onClick={onClick}
	>
		<div className={Classes.PageListItem_Icon}>{icon}</div>
		<div className={Classes.PageListItem_Title}>{title}</div>
	</div>
);

export const Separator: React.FC = () => (
	<div className={Classes.PageListSeparator} />
);
