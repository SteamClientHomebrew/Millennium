const axios = require('axios');
const fs = require('fs');

const github = {
    
    downloadFile: async (url: any, destinationPath: any) => {
        try {
            const response = await axios({
                method: 'get',
                url: url,
                responseType: 'stream',
            });
        
            // Ensure the destination directory exists
            const destinationDir = destinationPath.substring(0, destinationPath.lastIndexOf('/'));
            if (!fs.existsSync(destinationDir)) {
                fs.mkdirSync(destinationDir, { recursive: true });
            }
        
            // Create a writable stream and pipe the response data to it
            const writer = fs.createWriteStream(destinationPath);
            response.data.pipe(writer);
        
            // Return a promise that resolves when the download is complete
            return new Promise((resolve, reject) => {
                writer.on('finish', resolve);
                writer.on('error', reject);
            });
        } catch (error: any) {
            throw new Error(`Error downloading file: ${error.message}`);
        }
    },
    get_latest: async () => {
        return new Promise(async (resolve, reject) => {
            try {
                const apiUrl = 'https://api.github.com/repos/ShadowMonster99/millennium-steam-binaries/releases/latest';
      
                const response = await fetch(apiUrl);
                const releaseData = await response.json();
                
                if (response.ok) {
                    resolve(releaseData)
                } else {
                    reject(`github api status code ${response.status}`)
                }
            }
            catch (err: any) {
                reject(err)
            }
        })
    }
}

export default github