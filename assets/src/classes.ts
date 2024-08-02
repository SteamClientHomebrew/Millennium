import { findClassModule } from "@millennium/ui"

export const devClasses = findClassModule(m => m.richPresenceLabel && m.blocked) as any
export const pagedSettingsClasses = findClassModule(m => m.PagedSettingsDialog_PageList) as any
export const settingsClasses = findClassModule(m => m.SettingsTitleBar && m.SettingsDialogButton) as any
export const notificationClasses = findClassModule(m => m.GroupMessageTitle && !m.ShortTemplate && !m.TwoLine && !m.FriendIndicator && !m.AchievementIcon) as any;
