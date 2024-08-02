import React, { ReactNode } from "react";

interface SettingsDialogSubHeaderProps {
  children: ReactNode;
}

export const SettingsDialogSubHeader: React.FC<
  SettingsDialogSubHeaderProps
> = ({ children }) => <div className="SettingsDialogSubHeader">{children}</div>;
