const native = require("./build/Release/rc522.node");
const listeners = new Set();
let value = null;
let isInit = false;

module.exports = exports = function (options, callback) {
  listeners.add(callback);
  callback(value);

  if (!isInit) {
    isInit = true;
    native(options, function (newValue) {
      value = newValue;
      for (const callback of listeners) callback(value);
    });
  }

  return function () {
    listeners.delete(callback);
  };
};
