import { app, BrowserWindow, ipcMain, globalShortcut, shell } from 'electron';
import path from 'node:path'
import is_auto_installer from './modules/auto-installer';
import './modules/ipc-handler';

process.env.DIST = path.join(__dirname, '../dist')
process.env.VITE_PUBLIC = app.isPackaged ? process.env.DIST : path.join(process.env.DIST, '../public')

let win: BrowserWindow | null = null
const VITE_DEV_SERVER_URL = process.env['VITE_DEV_SERVER_URL']

function createWindow() {

  const auto = is_auto_installer()

  const width = auto ? 425 : 665
  const height = auto ? 175 : 460
  const resizable = !auto

  win = new BrowserWindow({
    width: width,
    height: height,
    frame: false,
    minWidth: width,
    minHeight: height,
    icon: path.join(process.env.VITE_PUBLIC, 'logo.png'),
    resizable: resizable,
    webPreferences: {
      nodeIntegration: true,
      preload: path.join(__dirname, 'preload.js'),
    },
  })
  
  win.webContents.on('will-navigate', (event, navigationUrl) => {
    event.preventDefault();
    shell.openExternal(navigationUrl);
  });

  // Test active push message to Renderer-process.
  win.webContents.on('did-finish-load', () => {
    win?.webContents.send('main-process-message', (new Date).toLocaleString())
  })

  if (VITE_DEV_SERVER_URL) {
    //win.webContents.openDevTools()
    win.loadURL(VITE_DEV_SERVER_URL)
  } else {
    // globalShortcut.register('CommandOrControl+Shift+I', () => {
    //   // Do nothing or handle as needed
    // });  
    // win.loadFile('dist/index.html')
    win.loadFile(path.join(process.env.DIST, 'index.html'))
  }

  // win.webContents.on('new-window', (event: Electron.Event, url: string) => {
  //   event.preventDefault();
  //   shell.openExternal(url);
  // });
}



// Quit when all windows are closed, except on macOS. There, it's common
// for applications and their menu bar to stay active until the user quits
// explicitly with Cmd + Q.
app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
    win = null
  }
})

app.on('activate', () => {
  // On OS X it's common to re-create a window in the app when the
  // dock icon is clicked and there are no other windows open.
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow()
  }
})

ipcMain.on('close', () => {
  app.quit()
  if (!VITE_DEV_SERVER_URL) {
    globalShortcut.unregister('CommandOrControl+Shift+I');
  }
})

app.whenReady().then(createWindow)

const send_message = (channel: string, message: any) => {
  win?.webContents.send(channel, message)
}

export { send_message } 
