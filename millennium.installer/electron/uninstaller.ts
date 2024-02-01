import { send_message } from './main.ts'
import steam from './modules/steam/steam-helpers.ts';
import github from './modules/github/api.ts';

const log = (message: string) => {
  console.log(message);
  send_message('message-update', message);
}

function delay(ms: number) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

const fs = require('fs').promises;

function deleteFile(filePath: any) {
  return new Promise((resolve, reject) => {
    fs.unlink(filePath)
      .then(() => {
        resolve(`File ${filePath} deleted successfully`);
      })
      .catch((error: any) => {
        reject(error);
      });
  });
}



const uninstaller = {
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
  uninstall: async () => { 
    github.get_latest().then(async (releaseData: any) => {
      log(`Preparing to uninstall Millennium v${releaseData.tag_name}...`);
      await delay(1000)

      if (releaseData.assets && releaseData.assets.length > 0) {
        log(`Found ${releaseData.assets.length} files to remove...`)
        await delay(1000)

        log(`Evaluating Steam Path...`) 
        steam.get_path().then(async (steamPath: any) => { 
          log(`Attempting to remove files...`)
          await delay(1000)

          var count: number = 0;

          const downloadPromises = releaseData.assets.map((asset: any) =>
            deleteFile(`${steamPath}\\${asset.name}`).then(() => { 
              count++;
            }).catch((error: any) => {
                return error.code
            })
          );

          Promise.all(downloadPromises)
          .then((message) => {
              if (count == releaseData.assets.length) { 
                  log(`Successfully uninstalled Millennium!`)

                  send_message('uninstall-message', JSON.stringify({success: true}));
              } else {
                  const enoent = message.every(element => element === 'ENOENT');
                  var err: string;

                  if (enoent) { 
                    err = `It appears that Millennium is not installed on your system.`      
                  } else {
                    err = `Oops! ${releaseData.assets.length - count} file(s) could not be deleted.`
                  }

                  send_message('uninstall-message', JSON.stringify({success: false, message: err}));
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
      send_message('uninstall-message', JSON.stringify({success: false, message: `Failed to connect: ${error.message}. A connection is needed to properly uninstall`}));
    })
  },
  start: () => {
    log('Checking if Steam is running...');

    uninstaller.check_steam().then(() => { 
      log('Warming Unintaller Core...');
      uninstaller.uninstall()    
    })
  }
}

export default uninstaller;