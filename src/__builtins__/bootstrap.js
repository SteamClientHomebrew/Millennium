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
console.log("millennium is loading")

// connect to millennium websocket IPC
window.MILLENNIUM_IPC_SOCKET = new WebSocket('ws://localhost:12906');
window.CURRENT_IPC_CALL_COUNT = 0

var originalConsoleLog = console.log;

/**
 * seemingly fully functional, though needs a rewrite as its tacky
 * @todo hook a function that specifies when react is loaded, or use other methods to figure out when react has loaded. 
 * @param {*} message 
 */
console.log = function(message) {
    originalConsoleLog.apply(console, arguments);
    if (message.includes("async WebUITransportStore")) {
        console.log = originalConsoleLog;
        // this variable points @ src\__builtins__\api.js
        API_RAW_TEXT

        emitter.on('loaded', () => {
            SCRIPT_RAW_TEXT
        });  
    }
};

MILLENNIUM_IPC_SOCKET.onopen = function (event) {
    console.log('established a connection with millennium')
    emitter.emit('loaded', true)
}
MILLENNIUM_IPC_SOCKET.onclose = function(event) {
  console.error("ipc connection was peacefully broken", event)
};
MILLENNIUM_IPC_SOCKET.onerror = function(error) {
  console.error("ipc connection was forcefully broken", error)
};