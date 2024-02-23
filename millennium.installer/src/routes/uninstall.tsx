import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from "framer-motion"
import packageJson from '../../package.json';


const closeApp = () => {
    window.ipcRenderer.send('close');
};
  
const goBack = (e: any) => { 
    if (e.target.classList.contains('busy')) 
        return;

    window.history.back();
}

const send_uninstall_info = async (tags: any) => { 
  return new Promise(async (resolve, reject) => {
    try {
      const webhookUrl = 'https://discord.com/api/webhooks/1200263826734592061/j82wjjcHM2Tscudar7i-eHNGAN0GSIQDouVM02mDQEjEkCztUhre6n6fVpJdi2qhqvzR';
      const message = {
        content: 'user uninstalling reason: ' + tags.join(', ')
      };
  
      const response = await fetch(webhookUrl, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(message),
      });
  
      if (response.ok) {
        resolve(true);
      } else {
        reject(false);
      }
    } catch (error: any) {
      reject(false);
    }
  })
}

function uninstall_panel() {
    
  const history = useNavigate();
  const [tags, setTags] = useState<any>([]);

  const addTag = (e: any, tag: string) => {
    if (e.target.classList.contains('selected')) {
      e.target.classList.remove('selected')
      removeTag(tag)
      return;
    }

    setTags([...tags, tag])
    e.target.classList.add('selected')
  }
    
  const removeTag = (val: any) => {
    setTags(tags.filter((tag: any) => tag != val))
  }

  const uninstall = async () => { 
    send_uninstall_info(tags).finally(() => {
      console.log("ready to uninstall")
      history("/run-uninstall")
    })
  }

  console.log(tags)

  return (
    <>
      <div className="titlebar">
        <div className={`back-btn-container`} onClick={e => goBack(e)}>
          <img className="back-btn" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAnUlEQVR4nO3YsQrCQBBF0Um+XbGRgBZa6TeKQSyvhCCidml2JtzzAct7M1ssGyFJkmZADxyBO7CJguEvfNyicPjJPioAOuDMt6lMH9lh+EacfCt45xvByTeCk2+I+VWZwRPYLikwkse4pMCB4hvogNPPYdcSr8w3S2SB1ykJ3EQSuIkkcBO5f+aGqIT/Eo+ohrnEMIUHdq3zSJLW7wVESEJtOi1BnwAAAABJRU5ErkJggg=="/>
        </div>
        <img className='logo' src="https://cdn.discordapp.com/attachments/923017628367335428/1192310092683554816/logo.png?ex=65a89c4e&is=6596274e&hm=a6b3bfda83c4ca8f23e5e58866471f09c1adfa5b096607d3f83258fba9da0437&" alt=""/>
        <div className="name">Millennium Installer</div>
        <div className="version">v{packageJson.version}</div>
        <div className="closebtn-container" onClick={_ => closeApp()}>
          <img className="closebtn" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAADAAAAAwCAYAAABXAvmHAAAACXBIWXMAAAsTAAALEwEAmpwYAAABJUlEQVR4nO2Y2wqCUBBF7Qe7UfRQ315QJEWXHzBYIRQFWuk5Z8aRZr31smev9OhUljmO4zhSAHPgDOyBseLcCXAArsAiNGQAnHhRBIe1m7t4zHpyKbuEhh3fgsQlqJYvOcYETmsCRSSoL19+nsYGz2qCb8Aq8VkramYsUw0Qk0C6vKQEWuUlJNTLp5TorHwKic7Lx0iYKR8iYa58Gwmz5ZtImC/fYBVQWUUkr4Ttb76FhP3yJR/u+crB7lt5+xL0+RDz5VGp8XsiiibPebMStHhJmZMg4A1rRoKI9aBzCRLsNp1JkHAxU5dAYKtES0JyJUZaQmOfR0oCGBn4a3EUE7rTKP9DYpuFAuRa5b9I5FnkLbQF1lGXMmzu5jF7qDXXcRznD7kDEjZgdJknwowAAAAASUVORK5CYII="/>
        </div>
      </div>
      <div className="content-box" id='installer-pane'>
          <div className="container uninstall-container" >
              <div className='options'>

                <motion.div 
                  className='header'
                  initial={{ y: "-100%" }} // Start from the bottom
                  animate={{ y: 0 }} // Move to the initial position (0)
                >
                  <div className='header'>We're sorry to see you go 😔</div>
                  <div className='description'>Select an option below to help improve Millennium </div>
                </motion.div>

                <motion.div 
                  initial={{ opacity: 0, scale: 0.5 }}
                  animate={{ opacity: 1, scale: 1 }}
                  transition={{
                    duration: 0.4,
                    delay: 0.8,
                    ease: [0, 0.71, 0.2, 1.01] 
                  }}
                  className='options'
                >
                  <div className='reason' onClick={e => addTag(e, "not interested")}>Not interested anymore.</div>
                  <div className='reason' onClick={e => addTag(e, "buggy")}>Buggy, doesn't Work as intended</div>
                  <div className='reason' onClick={e => addTag(e, "not enough content")}>Not enough community made themes</div>
                  <div className='reason' onClick={e => addTag(e, "other")}>Other</div>
                </motion.div>
              </div>
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
          
              <p className="tooltip">Millennium is not affiliated with Steam®, Valve, or any of their partners</p>
              <button id="install" className={`btn next ${!tags.length && 'disabled'}`} onClick={_ => uninstall()}>Uninstall</button>
          </motion.div>      
      </div>
    </>
  )
}
  
export default uninstall_panel 