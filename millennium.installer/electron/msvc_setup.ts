// import https from 'https';
import { https } from 'follow-redirects';
import fs from 'fs';
import { exec } from 'child_process';
import { send_message } from './main.ts';
import path from 'path';

const url = 'https://aka.ms/vs/17/release/vc_redist.x86.exe';
const fileName = 'vc_redist.x86.exe';
const tempFolderPath = 'C:\\temp';

async function install_msvc() {
    return new Promise((resolve, reject) => {
        send_message('message-update', `Installing Microsoft Visual C++ Redistributable. This might take a moment...`);
        
        // Ensure the temp folder exists
        if (!fs.existsSync(tempFolderPath)) {
            fs.mkdirSync(tempFolderPath, { recursive: true });
        }
        
        const filePath = path.join(tempFolderPath, fileName);
        
        // Download the file
        const file = fs.createWriteStream(filePath);
        https.get(url, function(response) {
            response.on('data', (chunk) => {
                file.write(chunk);
            });
        
            response.on('end', () => {
                file.close(() => {
                    console.log('File downloaded successfully.');
        
                    // Run the executable with parameter /quiet /norestart /log
                    const command = `"${filePath}" /quiet /norestart /log "C:\\temp\\log.txt"`;
                    const child = exec(command, { cwd: tempFolderPath });
                    
                    child.on('exit', (code, _) => {
                        console.log(`Child process exited with code ${code}`);
                        resolve(true);
                    });
                });
            });
        }).on('error', function(err) {
            fs.unlink(filePath, (err) => {
                if (err) console.error('Error deleting file:', err);
            });
            console.error('Error downloading file:', err);
            reject(err);
        });
    });
}

export default install_msvc;