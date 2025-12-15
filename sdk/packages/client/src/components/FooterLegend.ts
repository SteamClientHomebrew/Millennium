import { ReactNode } from 'react';

export enum GamepadButton {
  INVALID,
  OK,
  CANCEL,
  SECONDARY,
  OPTIONS,
  BUMPER_LEFT,
  BUMPER_RIGHT,
  TRIGGER_LEFT,
  TRIGGER_RIGHT,
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT,
  SELECT,
  START,
  LSTICK_CLICK,
  RSTICK_CLICK,
  LSTICK_TOUCH,
  RSTICK_TOUCH,
  LPAD_TOUCH,
  LPAD_CLICK,
  RPAD_TOUCH,
  RPAD_CLICK,
  REAR_LEFT_UPPER,
  REAR_LEFT_LOWER,
  REAR_RIGHT_UPPER,
  REAR_RIGHT_LOWER,
  STEAM_GUIDE,
  STEAM_QUICK_MENU,
}
export declare enum NavEntryPositionPreferences {
  FIRST,
  LAST,
  MAINTAIN_X,
  MAINTAIN_Y,
  PREFERRED_CHILD,
}
export interface GamepadEventDetail {
  button: number;
  is_repeat?: boolean;
  source: number;
}
export declare type ActionDescriptionMap = {
  [key in GamepadButton]?: ReactNode;
};
export declare type GamepadEvent = CustomEvent<GamepadEventDetail>;
export interface FooterLegendProps {
  actionDescriptionMap?: ActionDescriptionMap;
  onOKActionDescription?: ReactNode;
  onCancelActionDescription?: ReactNode;
  onSecondaryActionDescription?: ReactNode;
  onOptionsActionDescription?: ReactNode;
  onMenuActionDescription?: ReactNode;
  onButtonDown?: (evt: GamepadEvent) => void;
  onButtonUp?: (evt: GamepadEvent) => void;
  onOKButton?: (evt: GamepadEvent) => void;
  onCancelButton?: (evt: GamepadEvent) => void;
  onSecondaryButton?: (evt: GamepadEvent) => void;
  onOptionsButton?: (evt: GamepadEvent) => void;
  onGamepadDirection?: (evt: GamepadEvent) => void;
  onGamepadFocus?: (evt: GamepadEvent) => void;
  onGamepadBlur?: (evt: GamepadEvent) => void;
  onMenuButton?: (evt: GamepadEvent) => void;
}
