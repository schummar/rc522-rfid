const native = require("./build/Release/rc522.node");
const listeners = new Set();
let value = null;
let isInit = false;

module.exports = exports = function (options, callback) {
  listeners.add(callback);
  callback(value);

  if (!isInit) {
    isInit = true;

    if (typeof options.delay !== "number") options.delay = 100;
    if (typeof options.clockDivider !== "number") options.clockDivider = 512;
    if (typeof options.debug !== "boolean") options.debug = false;

    native(options, function (newValue) {
      value = newValue;
      for (const callback of listeners) callback(value);
    });
  }

  return function () {
    listeners.delete(callback);
  };
};
