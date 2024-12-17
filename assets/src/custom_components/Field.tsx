import React, { ReactNode } from "react";
import { fieldClasses } from "../classes";

const containerClasses = [
	fieldClasses.Field,
	fieldClasses.WithFirstRow,
	fieldClasses.VerticalAlignCenter,
	fieldClasses.WithDescription,
	fieldClasses.WithBottomSeparatorStandard,
	fieldClasses.ChildrenWidthFixed,
	fieldClasses.ExtraPaddingOnChildrenBelow,
	fieldClasses.StandardPadding,
	fieldClasses.HighlightOnFocus,
	"Panel",
].join(" ");

interface FieldProps {
	children: ReactNode;
	description: ReactNode;
	label: ReactNode;
}

/**
 * @note
 * Use this instead of the `@steambrew/client` one to prevent the
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
			<div className={fieldClasses.FieldChildrenWithIcon}>{children}</div>
		</div>
		<div className={fieldClasses.FieldDescription}>{description}</div>
	</div>
);
