import React, { ReactNode } from "react";
import { Classes, classMap, findClassModule } from "@millennium/ui";

const containerClasses = [
	Classes.Field,
	Classes.WithFirstRow,
	Classes.VerticalAlignCenter,
	Classes.WithDescription,
	Classes.WithBottomSeparatorStandard,
	Classes.ChildrenWidthFixed,
	Classes.ExtraPaddingOnChildrenBelow,
	Classes.StandardPadding,
	Classes.HighlightOnFocus,
	"Panel",
].join(" ");
const fieldClasses: any = findClassModule(
	(m) =>
		m.FieldLabel &&
		!m.GyroButtonPickerDialog &&
		!m.ControllerOutline &&
		!m.AwaitingEmailConfIcon,
);

interface FieldProps {
	children: ReactNode;
	description: ReactNode;
	label: ReactNode;
}

/**
 * @note
 * Use this instead of the `@millennium/ui` one to prevent the
 * `Assertion failed: Trying to use ConfigContext without a provider!  Add ConfigContextRoot to application.`
 * error.
 */
export const Field: React.FC<FieldProps> = ({
	children,
	description,
	label,
}) => (
	<div className={containerClasses}>
		<div className={fieldClasses.FieldLabelRow}>
			<div className={fieldClasses.FieldLabel}>{label}</div>
			<div className={classMap.FieldChildrenWithIcon}>{children}</div>
		</div>
		<div className={classMap.FieldDescription}>{description}</div>
	</div>
);
