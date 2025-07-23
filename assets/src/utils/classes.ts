/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2025 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
