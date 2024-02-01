import { ipcMain } from 'electron';
import is_auto_installer from './auto-installer';

import installer from '../installer';
import uninstaller from '../uninstaller';

ipcMain.handle('auto-installer', (_) => { 
    return is_auto_installer()
})
ipcMain.on('install', () => { 
    console.log('Installing...');
    installer.start(false);
})
ipcMain.on('install--force', () => { 
    console.log('Installing Millennium --force');
    installer.start(true);
})
ipcMain.on('uninstall', () => { 
    console.log('Uninstalling...');
    uninstaller.start();
})