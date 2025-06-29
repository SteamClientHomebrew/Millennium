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

import { DialogButton, DialogButtonPrimary, Field, IconsModule, ProgressBarWithInfo } from '@steambrew/client';
import { settingsClasses } from '../../utils/classes';
import { Component, ReactNode } from 'react';
import Markdown from 'markdown-to-jsx';
import { locale } from '../../../locales';
import { IconButton } from '../../components/IconButton';

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
	onUpdateClick: () => void;
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
}

export class UpdateCard extends Component<UpdateCardProps, UpdateCardState> {
	constructor(props: UpdateCardProps) {
		super(props);
		this.state = {
			showingMore: false,
		};

		this.handleToggle = this.handleToggle.bind(this);
		this.makeAnchorExternalLink = this.makeAnchorExternalLink.bind(this);
	}

	handleToggle() {
		this.setState((prevState) => ({ showingMore: !prevState.showingMore }));
	}

	private showInteractables() {
		const { isUpdating, statusText, progress, onUpdateClick } = this.props;

		if (isUpdating) {
			return (
				<ProgressBarWithInfo
					// @ts-ignore
					className="MillenniumUpdates_ProgressBar"
					sOperationText={statusText}
					nProgress={progress}
					nTransitionSec={1000}
				/>
			);
		}

		return (
			<IconButton onClick={onUpdateClick}>
				<IconsModule.Update key="download-icon" />
			</IconButton>
		);
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

		return (
			<div className="MillenniumUpdates_Description">
				<div>
					<b>{locale.updatePanelReleasedTag}</b> {update?.date}
				</div>
				<div>
					<b>{locale.updatePanelReleasePatchNotes}</b>&nbsp;
					<Markdown options={{ overrides: { a: { component: this.makeAnchorExternalLink } } }}>{update?.message}</Markdown>
				</div>
			</div>
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
			>
				{this.showInteractables()}
				<IconButton onClick={this.handleToggle} className="MillenniumUpdates_ExpandButton">
					<IconsModule.Carat direction="up" />
				</IconButton>
			</Field>
		);
	}
}
