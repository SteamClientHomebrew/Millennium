import { createHashRouter, RouterProvider } from 'react-router-dom';
import SelectPanel from './routes/user-select';

import InstallPanel from './routes/install';
import UninstallPanel from './routes/uninstall';

import Installer from './routes/installer';
import Uninstaller from './routes/uninstaller';
import AutoInstaller from './routes/auto-updater';

import { useEffect, useState } from 'react';

function App() {
  const router = createHashRouter([
    {
      path: "/",
      element: <SelectPanel/>
    },
    {
      path: "/install",
      element: <InstallPanel/>
    },
    {
      path: "/run-install",
      element: <Installer/>
    },
    {
      path: "/uninstall",
      element: <UninstallPanel/>
    },
    {
      path: "/run-uninstall",
      element: <Uninstaller/>
    }
  ]);

  const auto_router = createHashRouter([
    {
      path: "/",
      element: <AutoInstaller/>
    }
  ])

  const [auto_installer, setAutoInstaller] = useState({
    initial: false,
    auto_installer: undefined,
  });

  useEffect(() => {
    window.ipcRenderer.invoke('auto-installer').then((response: any) => {
      console.log('Response from main process:', response);
      setAutoInstaller({
        initial: true,
        auto_installer: response,
      })
    });
  }, [])

  return (
    <>
        {auto_installer.initial ? <RouterProvider router={auto_installer.auto_installer ? auto_router : router}></RouterProvider> : <></>}     
    </>
  );
}

export default App;