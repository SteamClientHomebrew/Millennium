import { DialogButton, DialogButtonPrimary, Field, IconsModule, ProgressBarWithInfo } from '@steambrew/client';
import { settingsClasses } from '../../utils/classes';
import { Component, ReactNode } from 'react';
import Markdown from 'markdown-to-jsx';
import { locale } from '../../../locales';

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
					style={{ padding: 'unset' }}
					className="UpdaterProgressBar"
					sOperationText={statusText}
					nProgress={progress}
					nTransitionSec={1000}
				/>
			);
		}

		return (
			<DialogButtonPrimary onClick={onUpdateClick} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
				Download <IconsModule.Download key="download-icon" />
			</DialogButtonPrimary>
		);
	}

	private makeAnchorExternalLink({ children, ...props }: { children: any; [key: string]: any }) {
		return (
			<a target="_blank" {...props}>
				{children}
			</a>
		);
	}

	RenderDescription() {
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
				className="MillenniumUpdateField"
				label={<div className="MillenniumUpdates_Label">{update.name}</div>}
				bottomSeparator={index === totalCount - 1 ? 'none' : 'standard'}
				{...(showingMore && { description: this.RenderDescription() })}
			>
				{this.showInteractables()}
				<DialogButton onClick={this.handleToggle} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`}>
					<IconsModule.Carat direction={showingMore ? 'up' : 'down'} />
				</DialogButton>
			</Field>
		);
	}
}
