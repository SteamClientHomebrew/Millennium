import { IconsModule, pluginSelf, toaster } from '@steambrew/client';
import { PyUpdateMillennium } from './utils/ffi';
import { deferredSettingLabelClasses } from './utils/classes';

interface UpdateProgress {
    current: number;
    total: number;
}

interface UpdateResult {
    success: boolean;
    error?: string;
}

type ProgressCallback = (progress: UpdateProgress) => void;

interface UpdateStatus {
    inProgress: boolean;
    error: string | null;
    progress: number;
}

/**
 * Handles the Millennium update process with progress tracking and error handling
 * @returns Promise<boolean> - true if update was successful, false otherwise
 */
export async function updateMillennium(): Promise<UpdateResult> {
    let currentProgress = 0;

    try {
        const downloadUrl = pluginSelf.millenniumUpdates?.platformRelease?.browser_download_url;

        if (!downloadUrl) {
            throw new Error('No download URL found for update');
        }

        // Show initial update notification
        toaster.toast({
            title: 'Millennium Update',
            body: 'Starting update process...',
            logo: <IconsModule.Loading className={deferredSettingLabelClasses.Icon} />,
            duration: 3000,
            critical: false,
        });

        // Start the update process
        const updateResult = await PyUpdateMillennium({ 
            downloadUrl,
            onProgress: ({ current, total }: UpdateProgress) => {
                const progressPercentage = Math.floor((current / total) * 100);
                
                // Only show notification if progress has increased by 25% or more
                if (progressPercentage >= currentProgress + 25) {
                    currentProgress = progressPercentage;
                    toaster.toast({
                        title: 'Millennium Update Progress',
                        body: `Downloading: ${progressPercentage}%`,
                        logo: <IconsModule.Loading className={deferredSettingLabelClasses.Icon} />,
                        duration: 2000,
                    });
                }
            }
        });

        if (!updateResult.success) {
            throw new Error(updateResult.error || 'Unknown error during update');
        }

        // Update successful
        pluginSelf.millenniumUpdates.updateInProgress = true;
        
        toaster.toast({
            title: 'Millennium Update Complete',
            body: 'Update downloaded successfully. Restart Steam to apply the update.',
            logo: <IconsModule.Checkmark className={deferredSettingLabelClasses.Icon} />,
            duration: 7000,
            critical: false,
            sound: 2,
        });

        return { success: true };

    } catch (error) {
        const errorMessage = error instanceof Error ? error.message : 'Unknown error occurred';
        
        toaster.toast({
            title: 'Millennium Update Error',
            body: errorMessage,
            logo: <IconsModule.ExclamationPoint className={deferredSettingLabelClasses.Icon} />,
            duration: 5000,
            critical: true,
            sound: 3,
        });

        // Log the error for debugging
        console.error('[Millennium Update Error]:', error);
        return { 
            success: false, 
            error: errorMessage 
        };
    }
}
