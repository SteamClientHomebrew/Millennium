import { findModuleExport } from "@millennium/ui";
import React, { CSSProperties, ReactNode } from "react";

interface TitleBarProps {
	bOSX?: boolean;
	bForceWindowFocused?: boolean;
	children?: ReactNode;
	className?: string;
	extraActions?: ReactNode;
	hideActions?: boolean;
	hideClose?: boolean;
	hideMin?: boolean;
	hideMax?: boolean;
	onClose?: (e: PointerEvent) => void;
	onMaximize?: () => void;
	onMinimize?: (e: PointerEvent) => void;
	popup?: Window;
	style?: CSSProperties;
}

export const TitleBar: React.FC<TitleBarProps> = findModuleExport(
	(m) =>
		typeof m === "function" &&
		m.toString().includes('className:"title-area-highlight"'),
);
