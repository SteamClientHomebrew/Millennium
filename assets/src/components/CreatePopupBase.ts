import { findModuleExport } from "@steambrew/client";

export const CreatePopupBase: any = findModuleExport(
	(m) =>
		typeof m === "function" &&
		m?.toString().includes("CreatePopup(this.m_strName"),
);
