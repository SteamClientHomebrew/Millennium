const { exec } = require('child_process');
const Registry = require('winreg');

const steam = {
    is_running: (query: any, cb: any) => {
        let platform = process.platform;
        let cmd = '';
        switch (platform) {
            case 'win32' : cmd = `tasklist`; break;
            case 'darwin' : cmd = `ps -ax | grep ${query}`; break;
            case 'linux' : cmd = `ps -A`; break;
            default: break;
        }
        exec(cmd, (_: any, stdout: any, __: any) => {
            cb(stdout.toLowerCase().indexOf(query.toLowerCase()) > -1);
        });
    },
    kill: () => {

        return new Promise((resolve, _) => {
            const processName = 'steam.exe';
            // Use taskkill command to forcefully terminate the process
            const command = `taskkill /F /IM ${processName}`;
            
            exec(command, (error: any, _: any, stderr: any) => {
                if (error) {
                    console.log(error.code == 128)


                    resolve(error.code == 128 ? 'Steam not running' : `Error killing process: ${stderr}`)
                } else {
                    resolve(`Steam closed successfully.`)
                }
            });
        })
    }, 
    get_path: () => {
        return new Promise((resolve, reject) => {
            const regKey = new Registry({
                hive: Registry.HKCU,
                key: '\\Software\\Valve\\Steam'
            });

            regKey.values((err: any, items: any) => {
            if (err) {
                reject(err);
            } else {
                const steamPathItem = items.find((item: any) => item.name === 'SteamPath');

                if (steamPathItem) {
                    resolve(steamPathItem.value);
                } else {
                    reject('SteamPath not found in registry');
                }
            }
            });
        });
    },
    start: () => {
        return new Promise((resolve, reject) => {
            steam.get_path().then((steamPath: any) => { 
                const childProcess = require('child_process');
                const executablePath = `${steamPath}\\steam.exe`;
                const options = { 
                    detached: true, 
                    stdio: 'ignore' 
                };
                
                childProcess.spawn(executablePath, null, options).unref();

                resolve("Started Steam")
            }).catch((error: any) => { 
                reject(error)
            })
        })
    }
}

export default steam