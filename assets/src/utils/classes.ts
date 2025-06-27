import { findClassModule } from '@steambrew/client';

export const contextMenuClasses = findClassModule((m) => m.ContextMenuMouseOverlay) as any;
export const deferredSettingLabelClasses = findClassModule((m) => m.DeferredSettingLabel) as any;
export const devClasses = findClassModule((m) => m.richPresenceLabel && m.blocked) as any;
export const fieldClasses: any = findClassModule((m) => m.FieldLabel && !m.GyroButtonPickerDialog && !m.ControllerOutline && !m.AwaitingEmailConfIcon);

export const pagedSettingsClasses = findClassModule((m) => m.PagedSettingsDialog_PageList) as any;
export const settingsClasses = findClassModule((m) => m.SettingsTitleBar && m.SettingsDialogButton) as any;
export const notificationClasses = findClassModule((m) => m.GroupMessageTitle && !m.ShortTemplate && !m.TwoLine && !m.FriendIndicator && !m.AchievementIcon) as any;
export const steamSuperNavClass = (findClassModule((m) => m.RootMenuButton && m.RootMenuBar && m.SteamButton) as any)?.SteamButton;
export const toolTipClasses = findClassModule((m) => m.HoverPosition && m.Ready && m.NoSpace) as any;
export const toolTipBodyClasses = findClassModule((m) => m.TextToolTip && m.ToolTipCustom && m.ToolTipTitle) as any;
export const DialogControlSectionClass = (findClassModule((m) => m.ActionSection && m.LibrarySettings) as any).ActionSection;
