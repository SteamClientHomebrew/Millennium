import { DialogButton, DialogButtonPrimary, Field, IconsModule, ProgressBarWithInfo } from '@steambrew/client';
import { settingsClasses } from '../../utils/classes';
import { Component, ReactNode, createRef } from 'react';
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
	private descriptionRef: React.RefObject<HTMLDivElement>;
	private descriptionHeight: number = 0;

	constructor(props: UpdateCardProps) {
		super(props);
		this.state = {
			showingMore: false,
		};

		this.descriptionRef = createRef();

		this.handleToggle = this.handleToggle.bind(this);
		this.makeAnchorExternalLink = this.makeAnchorExternalLink.bind(this);
	}

	componentDidMount() {
		this.measureDescriptionHeight();
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
			<DialogButtonPrimary onClick={onUpdateClick} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton}`} data-update-button>
				Update <IconsModule.Update key="download-icon" />
			</DialogButtonPrimary>
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
		const { showingMore } = this.state;
		const { update } = this.props;

		const containerStyle = {
			height: showingMore ? `${this.descriptionHeight}px` : '0px',
			overflow: 'hidden',
			transition: 'height 0.3s ease',
		};

		return (
			<div className={`MillenniumUpdates_Description ${showingMore ? 'expanded' : ''}`} style={containerStyle}>
				<div ref={this.descriptionRef}>
					<div>
						<b>{locale.updatePanelReleasedTag}</b> {update?.date}
					</div>
					<div>
						<b>{locale.updatePanelReleasePatchNotes}</b>&nbsp;
						<Markdown options={{ overrides: { a: { component: this.makeAnchorExternalLink } } }}>{update?.message}</Markdown>
					</div>
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
				description={this.renderDescription()}
			>
				{this.showInteractables()}
				<DialogButton onClick={this.handleToggle} className={`MillenniumIconButton ${settingsClasses.SettingsDialogButton} ${showingMore ? 'expanded' : ''}`}>
					<IconsModule.Carat direction="up" {...(showingMore && { style: { transform: 'rotate(180deg)' } })} />
				</DialogButton>
			</Field>
		);
	}
}
