const https = require('https');
const fs = require('fs');
const { exec } = require('child_process');
import { send_message } from './main.ts'
const path = require('path');

const url = 'https://download.microsoft.com/download/1/7/1/1718CCC4-6315-4D8E-9543-8E28A4E18C4C/dxwebsetup.exe';
const fileName = 'dxwebsetup.exe';
const tempFolderPath = 'C:\\temp';

// Download the file

async function install_dx() {
    return new Promise((resolve) => {
        send_message('message-update', `Installing Direct X9. This might take a moment...`)
        
        // Ensure the temp folder exists
        if (!fs.existsSync(tempFolderPath)) {
            fs.mkdirSync(tempFolderPath, { recursive: true });
        }
        
        const filePath = path.join(tempFolderPath, fileName);
        
        // Download the file
        const file = fs.createWriteStream(filePath);
        https.get(url, function(response: any) {
            response.pipe(file);
            file.on('finish', function() {
            file.close(() => {
                console.log('File downloaded successfully.');
        
                // Run the executable with parameter /Q
                const command = `"${filePath}" /Q`;
                const child = exec(command, { cwd: tempFolderPath });
                
                // Listen for the exit event
                child.on('exit', (code: any, _: any) => {
                    console.log(`Child process exited with code ${code}`);
                    resolve(true);
                // Here you can continue your code logic after the executable has finished running.
                });
            });
            });
        }).on('error', function(err: any) {
            fs.unlink(filePath); // Delete the file if an error occurs
            console.error('Error downloading file:', err);
        });
    })
}


export default install_dx