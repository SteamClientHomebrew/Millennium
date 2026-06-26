/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
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

import { DialogButton, Field, IconsModule, joinClassNames } from '@steambrew/sdk';
import { settingsClasses } from '../../utils/classes';
import { Component, createRef, ReactNode } from 'react';
import Markdown from 'markdown-to-jsx';
import { locale, formatString } from '../../utils/localization-manager';
import { IconButton, IconButtonWithText } from '../../components/IconButton';
import { MillenniumIcons } from '../../components/Icons';

interface UpdateProps {
	message: string;
	date: ReactNode;
	commit: ReactNode;
	name: ReactNode;
}

interface UpdateCardProps {
	update: UpdateProps;
	index: number;
	totalCount: number;
	isUpdating: boolean;
	progress: number;
	statusText: string;
	toolTipText?: string;
	onUpdateClick: () => void;
	disabled?: boolean;
	downloadSize?: number;
}

function formatDownloadSize(bytes: number): string {
	if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
	if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
	return `${bytes} B`;
}

export interface UpdateItemType {
	message: string;
	date: string;
	commit: string;
	native: string;
	name: string;
}

interface UpdateCardState {
	showingMore: boolean;
	initialized: boolean;
}

export class UpdateCard extends Component<UpdateCardProps, UpdateCardState> {
	private descriptionRef: React.RefObject<HTMLDivElement | null>;
	private descriptionHeight: number = 0;

	constructor(props: UpdateCardProps) {
		super(props);
		this.state = {
			showingMore: true,
			initialized: false,
		};

		this.descriptionRef = createRef();

		this.handleToggle = this.handleToggle.bind(this);
		this.makeAnchorExternalLink = this.makeAnchorExternalLink.bind(this);
	}

	componentDidMount() {
		if (this.descriptionRef.current) {
			this.descriptionHeight = this.descriptionRef.current.offsetHeight;
			this.setState({ initialized: true });
		}
	}

	componentDidUpdate(prevProps: UpdateCardProps, prevState: UpdateCardState) {
		// Re-measure if content or visibility changes
		if (prevProps.update?.message !== this.props.update?.message || prevState.showingMore !== this.state.showingMore) {
			this.measureDescriptionHeight();
		}
	}

	private measureDescriptionHeight() {
		if (this.descriptionRef.current) {
			this.descriptionHeight = this.descriptionRef.current.offsetHeight;
			this.forceUpdate(); // Needed to re-render with the measured height
		}
	}

	handleToggle() {
		this.setState((prevState) => ({ showingMore: !prevState.showingMore }));
	}

	private showInteractables() {
		const { isUpdating, statusText, onUpdateClick, update, toolTipText, downloadSize } = this.props;

		if (isUpdating) {
			return (
				<DialogButton className={joinClassNames(settingsClasses.SettingsDialogButton, 'MillenniumButton')}>
					<MillenniumIcons.LoadingSpinner />
					{statusText || locale.updatePanelIsUpdating}
				</DialogButton>
			);
		}

		const buttonText = downloadSize && downloadSize > 0 ? formatString(locale.strDownloadWithSize, formatDownloadSize(downloadSize)) : locale.strDownload;
		return <IconButtonWithText name="Download" text={buttonText} onClick={onUpdateClick} disabled={this.props.disabled} tooltip={`Update ${toolTipText || update.name}`} />;
	}

	private makeAnchorExternalLink({ children, ...props }: { children: any; [key: string]: any }) {
		return (
			<a target="_blank" rel="noopener noreferrer" {...props}>
				{children}
			</a>
		);
	}

	private renderDescription() {
		const { update } = this.props;
		const { initialized } = this.state;

		const style: React.CSSProperties = initialized ? { height: this.descriptionHeight } : { height: 'auto', transition: 'none' };

		return (
			<div className="MillenniumUpdates_Description" style={style}>
				<div ref={this.descriptionRef}>
					<div>
						<b>{locale.updatePanelReleasedTag}</b> {update?.date}
					</div>
					<div>
						<b>{locale.updatePanelReleasePatchNotes}</b>&nbsp;
						<Markdown options={{ overrides: { a: { component: this.makeAnchorExternalLink } } }}>{update?.message.replace(/#[\s\S]*?\n\n\n/, '')}</Markdown>
					</div>
				</div>
			</div>
		);
	}

	private renderChildren() {
		const { showingMore } = this.state;
		const { disabled } = this.props;

		if (disabled) {
			return (
                <DialogButton className={joinClassNames("MillenniumButton", settingsClasses.SettingsDialogButton)} disabled={true}>
					<IconsModule.Clock />
					{locale.updateCompletePendingRestart}
				</DialogButton>
			);
		}

		return (
			<>
				{this.showInteractables()}
				<IconButton
					name="KaratDown"
					onClick={this.handleToggle}
					className="MillenniumUpdates_ExpandButton"
					disabled={this.props.disabled}
					tooltip={showingMore ? locale.strCollapse : locale.strExpand}
				/>
			</>
		);
	}

	render() {
		const { showingMore } = this.state;
		const { update, index, totalCount } = this.props;

		return (
			<Field
				key={index}
				className="MillenniumUpdates_Field"
				label={update.name}
				bottomSeparator={index === totalCount - 1 ? 'none' : 'standard'}
				description={this.renderDescription()}
				data-expanded={showingMore}
				disabled={this.props.disabled}
			>
				{this.renderChildren()}
			</Field>
		);
	}
}
