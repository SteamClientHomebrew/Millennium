import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from "framer-motion"

const closeApp = () => {
  console.log("requesting to close")
  window.ipcRenderer.send('close');
};

function select_panel() {
    
    const [selected, setSelected] = useState("install");

    const select = (e: any) => { 
      //@ts-ignore
      document.querySelector(".left-div").classList.remove("selected");
      //@ts-ignore
      document.querySelector(".right-div").classList.remove("selected");
  
      if (e == "install") {
        //@ts-ignore
        document.querySelector(".left-div").classList.add("selected");
        setSelected("install")
      }
      if (e == "remove") {
        //@ts-ignore
        document.querySelector(".right-div").classList.add("selected");
        setSelected("remove")
      }
    }

    const history = useNavigate();
    
    const next = () => { 
      switch (selected) { 
        case "install": 
          history("/install");
          break;
        case "remove": 
          history("/uninstall");
          break;
        // add other cases if needed
        default:
          // handle default case
      }
    }
  
    // rest of your component
  
    return (
      <>
        <div className="titlebar">
          <img className='logo' src="https://cdn.discordapp.com/attachments/923017628367335428/1192310092683554816/logo.png?ex=65a89c4e&is=6596274e&hm=a6b3bfda83c4ca8f23e5e58866471f09c1adfa5b096607d3f83258fba9da0437&" alt=""/>
          <div className="name">Millennium Installer</div>
          <div className="version">v1.0.6</div>
          <div className="closebtn-container" onClick={_ => closeApp()}>
            <img className="closebtn" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAACXBIWXMAAAsTAAALEwEAmpwYAAABJUlEQVR4nO2Y2wqCUBBF7Qe7UfRQ315QJEWXHzBYIRQFWuk5Z8aRZr31smev9OhUljmO4zhSAHPgDOyBseLcCXAArsAiNGQAnHhRBIe1m7t4zHpyKbuEhh3fgsQlqJYvOcYETmsCRSSoL19+nsYGz2qCb8Aq8VkramYsUw0Qk0C6vKQEWuUlJNTLp5TorHwKic7Lx0iYKR8iYa58Gwmz5ZtImC/fYBVQWUUkr4Ttb76FhP3yJR/u+crB7lt5+xL0+RDz5VGp8XsiiibPebMStHhJmZMg4A1rRoKI9aBzCRLsNp1JkHAxU5dAYKtES0JyJUZaQmOfR0oCGBn4a3EUE7rTKP9DYpuFAuRa5b9I5FnkLbQF1lGXMmzu5jF7qDXXcRznD7kDEjZgdJknwowAAAAASUVORK5CYII="/>
          </div>
        </div>
        <div className="content-box" id="select-pane">
          <div className="options-container">
            <motion.div 
              initial={{ opacity: 0, scale: 0.5 }}
              animate={{ opacity: 1, scale: 1 }}
              transition={{
                duration: 0.4,
                delay: 0.8,
                ease: [0, 0.71, 0.2, 1.01]
              }}
              className='container-inner'
            >
              <div className="left-div selected" onClick={_ => select('install')}>
                <div className="header">Install</div>
                <div className="description">Integrate Millennium into Steam.</div>
              </div>
            
              <div className="right-div" onClick={_ => select('remove')}>
                <div className="header">Remove</div>
                <div className="description">Uninstall Millennium from Steam. Does not remove user settings</div>
              </div>
            </motion.div>
            {/* Expiremental (not really a fan of the look) */}
            {/* <div className="status-info">
              <div className='status-info-inner'>
                <div className={`color`}></div>
                <div className='description'>{auto_installer ? 'auto-installer' : 'not'}</div>
              </div>
            </div> */}
          </div>
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
              Millennium is not affiliated with Steam®, Valve, or any of their partners</p>
            <button id="install" className="btn next" onClick={_ => next()}>Next</button>
          </motion.div>
        </div>
        
      </>
    )
  }
  
  export default select_panel 