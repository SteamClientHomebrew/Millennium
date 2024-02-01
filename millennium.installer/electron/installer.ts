import steam from './modules/steam/steam-helpers.ts';
import github from './modules/github/api.ts';
import { send_message } from './main.ts'

const log = (message: string) => {
		console.log(message);
    send_message('message-update', message);
}

function delay(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

const installer = {
    check_steam: () => {
			return new Promise(async (resolve) => {
				const checkStatus = async () => {
					steam.is_running('steam.exe', (status: boolean) => {
						if (status) {
							send_message('message-update', 'Steam is running, waiting for exit...');
							setTimeout(checkStatus, 1000); 
						} else {
							resolve(true);
						}
					});
				};
				checkStatus();
			});
    },
    install: async () => { 
			github.get_latest().then(async (releaseData: any) => {
				send_message('update-version', releaseData.tag_name)
				log(`Downloading Millennium v${releaseData.tag_name}...`);
				await delay(1000)		

				if (releaseData.assets && releaseData.assets.length > 0) {
					log(`Querying ${releaseData.assets.length} files to download...`)
					await delay(1000)

					log(`Evaluating Steam Path...`) 
					steam.get_path().then(async (steamPath: any) => { 
						log(`Downloading File Queury...`)
						await delay(1000)
	
						var count: number = 0;
	
						const downloadPromises = releaseData.assets.map((asset: any) =>
							github.downloadFile(asset.browser_download_url, `${steamPath}\\${asset.name}`).then(() => { 
								count++;
							})
						);
					
						Promise.all(downloadPromises).then(() => {
							if (count == releaseData.assets.length) { 
								log(`Successfully installed Millennium!`)
	
								send_message('install-message',  JSON.stringify({success: true}));
								steam.start().then((message: any) => {
									log(message)
									send_message('install-message',  JSON.stringify({success: true}));
								}).catch((error: any) => {
									send_message('install-message',  JSON.stringify({
										success: false, 
										message: `Install succeeded but failed to start Steam: ${error}`
									}));
								})

							} else {
								log(`Failed to install Millennium!, ${releaseData.assets.length - count} file(s) could not be downloaded.`)
							}
						})
					}).catch((error: any) => {
						send_message('install-message', JSON.stringify({success: false, message: `Couldn't resolve Steam path. ${error}`}));
					});
	
				} else {
					console.log('No download files found in the release.');
				}
			}).catch((error: any) => {
				console.error('Error fetching release data:', error.message);
				send_message('install-message', JSON.stringify({success: false, message: `Failed to connect: ${error.message}. Ensure a proper internet connection.`}));
			})
    },
    start: (force_close: boolean) => {
		log('Checking if Steam is running...');

		steam.kill().then(async (message: any) => {
			log(message);	
			await delay(1000)
		});

		installer.check_steam().then(() => { 
			log('Warming Installer Core...');
			installer.install()   
		})
    }
}

export default installer;