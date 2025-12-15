export interface Stats {
    // param0 - AppDetailsReviewSection, Showcases, LibraryReviewSpotlight
    // param1 -
    // AppDetailsReviewSection: PositiveClicked, NegativeClicked, NeutralClicked, PositiveReviewPosted, NegativeReviewPosted, EditClicked, ReviewCanceled
    // LibraryReviewSpotlight: ReviseClicked, PositiveClicked, ReviseCloseClicked, NegativeClicked, PositiveRevisePosted, NegativeRevisePosted, ReviseCanceled, ReviewCanceled, CloseClicked
    // Showcases: Delete, Save-Modify, Save-New
    RecordActivationEvent(param0: string, param1: string): void;

    RecordDisplayEvent(param0: boolean, param1: string, param2: string): void;
}
