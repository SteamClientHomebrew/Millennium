import { useState, useEffect } from 'react';
import { motion } from "framer-motion"

const closeApp = () => {
    window.ipcRenderer.send('close');
};
  
const goBack = (e: any) => { 
    if (e.target.classList.contains('busy')) 
        return;

    window.history.back();
}

function uninstall_panel() {
    
    const [status, setStatus] = useState("Starting Uninstaller...");
    const [install_status, setInstallStatus] = useState(true);
    const [failed, setFailed] = useState<any>({});

    window.ipcRenderer.on('message-update', (_, arg) => {
        console.log('Received response:', arg);
        // Handle the response from the main process
        setStatus(arg)
    });

    window.ipcRenderer.on('uninstall-message', (_, arg) => {
        console.log('Received response:', arg);

        const json = JSON.parse(arg)

        if (!json.success) {
            setFailed({fail: true, message: json.message})
        }
        setInstallStatus(false)
    });

    useEffect(() => { 
        window.ipcRenderer.send('uninstall');
    }, [])

    return (
      <>
        <div className="titlebar">
          <div className={`back-btn-container ${install_status ? 'busy' : ''}`} onClick={e => goBack(e)}>
            <img className="back-btn" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAnUlEQVR4nO3YsQrCQBBF0Um+XbGRgBZa6TeKQSyvhCCidml2JtzzAct7M1ssGyFJkmZADxyBO7CJguEvfNyicPjJPioAOuDMt6lMH9lh+EacfCt45xvByTeCk2+I+VWZwRPYLikwkse4pMCB4hvogNPPYdcSr8w3S2SB1ykJ3EQSuIkkcBO5f+aGqIT/Eo+ohrnEMIUHdq3zSJLW7wVESEJtOi1BnwAAAABJRU5ErkJggg=="/>
          </div>
          <img className='logo' src="https://cdn.discordapp.com/attachments/923017628367335428/1192310092683554816/logo.png?ex=65a89c4e&is=6596274e&hm=a6b3bfda83c4ca8f23e5e58866471f09c1adfa5b096607d3f83258fba9da0437&" alt=""/>
          <div className="name">Millennium Installer</div>
          <div className="version">v1.0.6</div>
          <div className="closebtn-container" onClick={_ => closeApp()}>
            <img className="closebtn" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAACXBIWXMAAAsTAAALEwEAmpwYAAABJUlEQVR4nO2Y2wqCUBBF7Qe7UfRQ315QJEWXHzBYIRQFWuk5Z8aRZr31smev9OhUljmO4zhSAHPgDOyBseLcCXAArsAiNGQAnHhRBIe1m7t4zHpyKbuEhh3fgsQlqJYvOcYETmsCRSSoL19+nsYGz2qCb8Aq8VkramYsUw0Qk0C6vKQEWuUlJNTLp5TorHwKic7Lx0iYKR8iYa58Gwmz5ZtImC/fYBVQWUUkr4Ttb76FhP3yJR/u+crB7lt5+xL0+RDz5VGp8XsiiibPebMStHhJmZMg4A1rRoKI9aBzCRLsNp1JkHAxU5dAYKtES0JyJUZaQmOfR0oCGBn4a3EUE7rTKP9DYpuFAuRa5b9I5FnkLbQF1lGXMmzu5jF7qDXXcRznD7kDEjZgdJknwowAAAAASUVORK5CYII="/>
          </div>
        </div>
        <div className="content-box" id='installer-pane'>
  
            {install_status ? 
            <>
            <motion.div 
                transition={{
                    delay: .5,
                }}
                initial={{ y: "-100vh" }} // Start from the bottom
                animate={{ y: 0 }} // Move to the initial position (0)
                className='installer-box'
            >
                <svg className="spinner">
                    <circle>
                        <animateTransform attributeName="transform" type="rotate" values="-90;810" keyTimes="0;1" dur="2s" repeatCount="indefinite"></animateTransform>
                        <animate attributeName="stroke-dashoffset" values="0%;0%;-157.080%" calcMode="spline" keySplines="0.61, 1, 0.88, 1; 0.12, 0, 0.39, 0" keyTimes="0;0.5;1" dur="2s" repeatCount="indefinite"></animate>
                        <animate attributeName="stroke-dasharray" values="0% 314.159%;157.080% 157.080%;0% 314.159%" calcMode="spline" keySplines="0.61, 1, 0.88, 1; 0.12, 0, 0.39, 0" keyTimes="0;0.5;1" dur="2s" repeatCount="indefinite"></animate>
                    </circle>
                </svg>
                <div className='installer-details'>
                    <div className='installer-header'>Uninstalling Millennium.</div>
                    <div className='installer-text'>{status}</div>
                </div>
            </motion.div>
            </> : 
            <>
            {failed.fail ?
                <div className="container" id="install-success">
                    <div className='header'>❌ Failed to uninstall Millennium</div>
                    <div className='description'>{failed.message}</div>
                </div> 
                :
                <div className="container" id="install-success">
                    <div className='header'>You're all set! Millennium was uninstalled.</div>
                    <div className='description'>You're themes and presets haven't been touched 😊. We're sorry to see you go.</div>
                </div>
            }
            <motion.div 
                initial={{ y: "100%" }} // Start from the bottom
                animate={{ y: 0 }} // Move to the initial position (0)
                style={{
                  position: "absolute",
                  bottom: 0,
                  left: 0,
                  right: 0,
                }}
                transition={{
                    delay: 0.2,
                }}
                className="footer-content"
            >   
            
                <p className="tooltip">
                    <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAACXBIWXMAAAsTAAALEwEAmpwYAAACjklEQVR4nO1ZyW4TQRAdARduLCe2E/AHbDd/AFI89SpdZZL/YD04h+BciQDxD2xCJArfAcoNLiwHUGKuOFyCHrFZZEUznmnPDMJPKsmS5Zn3uqq7qp+TZIYZZiiNbrd7QMSuiPptwJ4JbBOwrwL7zhh+3hT1p4DdUu1c5m+SuiGyeFrUVgT+Ceq7k4V9FPVeCOFU5cTTND0O+COB70xOfCwGgD8MIRyrhLzMWwfq2xGI/x2wLdVgUyPearUOcdWjE9cxIQ/4rqjkQwiHAVubOnndC1F/yXfGIn8Qas+rIo/fmViLkolKykb3y4Tdj7FhayGPkQjpzBciz2ONJ0PdAqC+zWO7htKxdTYpNjvANiotJWGHLdmk+IzR89ptP1MyC4O5uc7J/ALUVsrXblQBu4DdzX1sFpttxl64QRF75O1V6QWBv881AHKqLE1+SpGGcClTAOB36iaK/bKgfiNbQB1dV/OGPckuIV5GYqwW/G2a+lkGP0cRAH+TQ4D3I6W79+uZ6r04Amwrj4CdSAKW/hCwFKmMBv+FgH5jBSBXCUXaxFMR4NmbmLZIgwU8zs4AvZ3GCrDrmQJoOjVVgGrnQqYADkxQ+9A0AZJ3mIvVeKILUFvORf7nC3mLomPWHAGDiS40BO2+xowSsNWkmP9Z/FLPAS6EcA64dh5q74oLsC9XFxaOJkVAr7Js7ZaNVB1JGdCrrIu8qN1LoliLkbpzLdYiQaOVhmuF5F9EM3dH4GrQYKqA/CqznkwL9Cp5MsQnb59Lb9i84LE2zMag9EaFfeOqt9vtI0nVYHekY1ZkduJsI2rLIosnkrrBIYumE+A3aX3w4sGbHa+nw+gL7DW/o7cjYhcb8TfrDDMk/z5+AHLrqx1z8KkFAAAAAElFTkSuQmCC"/>
                    Millennium is not affiliated with Steam®, Valve, or any of their partners
                </p>
                <button id="install" className="btn next" onClick={_ => closeApp()}>Finish</button>
            </motion.div>
            </>
            }
            
        </div>
      </>
    )
  }
  
  export default uninstall_panel 