// @ts-check

/** Define some node-specific variables */
// var process = process, require = require;

/**
 * @typedef {Object} PolBasicBinding
 * @property {(...data:any) => void} print
 */

/**
 * @typedef {Object} PolNodeBinding - creates a new type named 'SpecialType'
 * @property {configure} configure - Configure the environment.
 * @property {start} start - Start the thread-safe function.
 */

/**
 * @callback configure
 * @param {any} exports - Exports provided in JavaScript to be used inside the core. This includes
 * the `require` reference, allowing use of `require('./script.js')` in the core.
 * @returns {boolean} - True if configured correctly. False otherwise.
 */

/**
 * Start the thread-safe function.
 * @callback start
 * @param {function} coreToJsCallback - Callback executed from core asynchronously.
 * @param {number} maxQueueSize - Max Queue Size. Use 0 for no limit.
 * @returns {boolean} - True if successfully started thread-safe function. False otherwise.
 */

/** @type {PolNodeBinding} */
const pol = process._linkedBinding("pol");

/** @type {PolBasicBinding} */
const basic = process._linkedBinding("basic");

pol.configure({
  require,
  gc: global.gc,
  wrapper: require("./wrapper"),
  scriptloader: require("./scriptloader")
});

let coreCallback = function(args) {
  basic.print("Received call from core " /**  + args */);
};

process.on('uncaughtException', (err) => {
  console.error('There was an uncaught error!!!', err);
  // here i suppose we should trigger a server shutdown? is it this drastic?
  // unfortunately since we do not create an _actual_  new node context
  process.exit(1) //mandatory (as per the Node docs)
});

if (!pol.start(coreCallback, 0)) {
  basic.print("Could not start TSFN!");
} else {
  basic.print("Started TSFN. main.js exiting");
}

/**
 * This script has now ended execution. The node main thread will wait until the thread-safe
 * function created above is released from the non-Node thread (ie, the Core)
 */
