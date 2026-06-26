import { ReactNode } from 'react';

export interface ItemProps {
    label?: ReactNode;
    description?: ReactNode;
    children?: ReactNode;
    layout?: 'below' | 'inline';
    icon?: ReactNode;
    bottomSeparator?: 'standard' | 'thick' | 'none';
    childrenContainerWidth?: 'min' | 'max' | 'fixed'; // Does not work with layout==='below'
    indentLevel?: number;
    tooltip?: string;
    highlightOnFocus?: boolean;
}
