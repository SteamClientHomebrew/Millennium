import { useState, useEffect } from 'react';

const closeApp = () => {
    window.ipcRenderer.send('close');
};

function auto_install_panel() {
    
    const [status, setStatus] = useState("Starting Installer...");
    const [install_status, setInstallStatus] = useState(true);
    const [failed, setFailed] = useState<any>({});

    window.ipcRenderer.on('message-update', (_, arg) => {
        console.log('Message Update: ', arg);
        setStatus(arg)
    });

    window.ipcRenderer.on('install-message', (_, arg) => {
        console.log('Received response:', arg);
        const json = JSON.parse(arg)

        if (!json.success) {
            setFailed({fail: true, message: json.message})
            setInstallStatus(false)
        } else {
            closeApp()
        }
    });

    useEffect(() => { 
        window.ipcRenderer.send('install--force');
    }, [])

    return (
      <>
        <div className="titlebar">
          <div className="closebtn-container" onClick={_ => closeApp()}>
            <img className="closebtn" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAACXBIWXMAAAsTAAALEwEAmpwYAAABJUlEQVR4nO2Y2wqCUBBF7Qe7UfRQ315QJEWXHzBYIRQFWuk5Z8aRZr31smev9OhUljmO4zhSAHPgDOyBseLcCXAArsAiNGQAnHhRBIe1m7t4zHpyKbuEhh3fgsQlqJYvOcYETmsCRSSoL19+nsYGz2qCb8Aq8VkramYsUw0Qk0C6vKQEWuUlJNTLp5TorHwKic7Lx0iYKR8iYa58Gwmz5ZtImC/fYBVQWUUkr4Ttb76FhP3yJR/u+crB7lt5+xL0+RDz5VGp8XsiiibPebMStHhJmZMg4A1rRoKI9aBzCRLsNp1JkHAxU5dAYKtES0JyJUZaQmOfR0oCGBn4a3EUE7rTKP9DYpuFAuRa5b9I5FnkLbQF1lGXMmzu5jF7qDXXcRznD7kDEjZgdJknwowAAAAASUVORK5CYII="/>
          </div>
        </div>
        <div className="content-box" id='installer-pane'>
  
            {install_status ? 
            <>
            <div className='installer-box'>
                <svg className="spinner">
                    <circle>
                        <animateTransform attributeName="transform" type="rotate" values="-90;810" keyTimes="0;1" dur="2s" repeatCount="indefinite"></animateTransform>
                        <animate attributeName="stroke-dashoffset" values="0%;0%;-157.080%" calcMode="spline" keySplines="0.61, 1, 0.88, 1; 0.12, 0, 0.39, 0" keyTimes="0;0.5;1" dur="2s" repeatCount="indefinite"></animate>
                        <animate attributeName="stroke-dasharray" values="0% 314.159%;157.080% 157.080%;0% 314.159%" calcMode="spline" keySplines="0.61, 1, 0.88, 1; 0.12, 0, 0.39, 0" keyTimes="0;0.5;1" dur="2s" repeatCount="indefinite"></animate>
                    </circle>
                </svg>
                <div className='installer-details'>
                    <div className='installer-header'>Updating Millennium...</div>
                    <div className='installer-text'>{status}</div>
                </div>
            </div>
            </> : 
            <>
            {failed.fail ?
                <div className="installer-details" id="install-success">
                    <div className='header'>Failed to update Millennium ☹️</div>
                    <div className='description'>{failed.message}</div>
                </div> 
                :
                <div className="installer-details" id="install-success">
                    <div className='header'>You're all set! Thanks for using Millennium 💖</div>
                    <div className='description'>Start Steam to get further instruction.</div>
                </div>
            }
            </>
            }
            
        </div>
      </>
    )
  }
  
  export default auto_install_panel 