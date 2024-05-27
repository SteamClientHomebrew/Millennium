/**
 * This module is in place to load all plugin frontends
 * @ src\__builtins__\bootstrap.js
 */

class EventEmitter {
    constructor() {
      this.events = {};
      this.pendingEvents = {}; // Store emitted events temporarily
    }
  
    on(eventName, listener) {
      this.events[eventName] = this.events[eventName] || [];
      this.events[eventName].push(listener);
  
      // If there are pending events for this eventName, emit them now
      if (this.pendingEvents[eventName]) {
        this.pendingEvents[eventName].forEach(args => this.emit(eventName, ...args));
        delete this.pendingEvents[eventName];
      }
    }
  
    emit(eventName, ...args) {
      if (this.events[eventName]) {
        this.events[eventName].forEach(listener => listener(...args));
      } else {
        // If there are no listeners yet, store the emitted event for later delivery
        this.pendingEvents[eventName] = this.pendingEvents[eventName] || [];
        this.pendingEvents[eventName].push(args);
      }
    }
  
    off(eventName, listener) {
      if (this.events[eventName]) {
        this.events[eventName] = this.events[eventName].filter(registeredListener => registeredListener !== listener);
      }
    }
}
const emitter = new EventEmitter();
console.log('%c Millennium ', 'background: black; color: white', "Bootstrapping modules...");

// connect to millennium websocket IPC
window.CURRENT_IPC_CALL_COUNT = 0;

function connectIPCWebSocket() {
    window.MILLENNIUM_IPC_SOCKET = new WebSocket('ws://localhost:12906');

    window.MILLENNIUM_IPC_SOCKET.onopen = function(event) {
        console.log('%c Millennium ', 'background: black; color: white', "Successfully connected to Millennium!");
        emitter.emit('loaded', true);
    };

    window.MILLENNIUM_IPC_SOCKET.onclose = function(event) {
        console.error('IPC connection was peacefully broken.', event);
        setTimeout(connectIPCWebSocket, 100); 
    };

    window.MILLENNIUM_IPC_SOCKET.onerror = function(error) {
        console.error('IPC connection was forcefully broken.', error);
    };
}

connectIPCWebSocket();

// var originalConsoleLog = console.log;

/**
 * seemingly fully functional, though needs a rewrite as its tacky
 * @todo hook a function that specifies when react is loaded, or use other methods to figure out when react has loaded. 
 * @param {*} message 
 */
function __millennium_module_init__() {
  if (window.SP_REACTDOM === undefined) {
    setTimeout(() => __millennium_module_init__(), 1)
  }
  else {
    console.log('%c Millennium ', 'background: black; color: white', "Bootstrapping builtin modules...");
    // this variable points @ src_builtins_\api.js

    emitter.on('loaded', () => {
      SCRIPT_RAW_TEXT
    });
  }
}
__millennium_module_init__()