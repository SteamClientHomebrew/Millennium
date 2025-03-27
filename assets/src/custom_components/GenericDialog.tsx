import { ModalPosition } from '@steambrew/client';
import { ReactNode } from 'react';

interface GenericConfirmDialogProps {
	children: ReactNode;
}

export const GenericConfirmDialog = ({ children }: GenericConfirmDialogProps) => (
	<ModalPosition>
		<div className="DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically">
			<div className="DialogContent_InnerWidth">{children}</div>
		</div>
	</ModalPosition>
);
