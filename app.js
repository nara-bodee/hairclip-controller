// ======================================================
// BLE CONFIG
// ======================================================

const SERVICE_UUID =
  "c7a10001-6c9e-4d5d-a001-123456789abc";

const CHARACTERISTIC_UUID =
  "c7a10002-6c9e-4d5d-a001-123456789abc";

const AUTO_REFRESH_INTERVAL =
  5000;


// ======================================================
// BLE STATE
// ======================================================

let device = null;
let characteristic = null;

let isConnecting = false;
let manualDisconnect = false;

let autoRefreshTimer = null;

let bleQueue =
  Promise.resolve();


// ======================================================
// RUNTIME UI STATE
// ======================================================

let runtimeActiveUI =
  false;

let runtimeServerSeconds =
  0;

let runtimeLastSyncTime =
  Date.now();

let runtimeClockTimer =
  null;


// ======================================================
// ELEMENTS
// ======================================================

const connectButton =
  document.getElementById(
    "connectButton"
  );

const sendButton =
  document.getElementById(
    "sendButton"
  );

const messageInput =
  document.getElementById(
    "messageInput"
  );

const scrollToggle =
  document.getElementById(
    "scrollToggle"
  );

const displayToggle =
  document.getElementById(
    "displayToggle"
  );

const statusText =
  document.getElementById(
    "statusText"
  );

const statusBadge =
  document.getElementById(
    "statusBadge"
  );

const characterCount =
  document.getElementById(
    "characterCount"
  );

const log =
  document.getElementById(
    "log"
  );

const iconButtons =
  document.querySelectorAll(
    ".icon-button"
  );


const batteryCard =
  document.getElementById(
    "batteryCard"
  );

const batteryPercentElement =
  document.getElementById(
    "batteryPercent"
  );

const batteryVoltageElement =
  document.getElementById(
    "batteryVoltage"
  );

const batteryFill =
  document.getElementById(
    "batteryFill"
  );

const batteryNote =
  document.getElementById(
    "batteryNote"
  );

const batteryStatus =
  document.getElementById(
    "batteryStatus"
  );

const batteryStatusIcon =
  document.getElementById(
    "batteryStatusIcon"
  );

const batteryWarning =
  document.getElementById(
    "batteryWarning"
  );

const batteryWarningTitle =
  document.getElementById(
    "batteryWarningTitle"
  );

const batteryWarningText =
  document.getElementById(
    "batteryWarningText"
  );


const runtimeElapsed =
  document.getElementById(
    "runtimeElapsed"
  );

const runtimeState =
  document.getElementById(
    "runtimeState"
  );

const runtimeStart =
  document.getElementById(
    "runtimeStart"
  );

const runtimeResetButton =
  document.getElementById(
    "runtimeResetButton"
  );


// ======================================================
// HELPERS
// ======================================================

function wait(ms) {
  return new Promise(
    resolve =>
      setTimeout(
        resolve,
        ms
      )
  );
}


function isConnected() {
  return Boolean(
    device &&
    device.gatt &&
    device.gatt.connected &&
    characteristic
  );
}


function updateCharacterCount() {
  characterCount.textContent =
    `${messageInput.value.length} / 100`;
}


// ======================================================
// BLE QUEUE
// ======================================================

function enqueueBleOperation(
  operation
) {
  const next =
    bleQueue.then(
      operation,
      operation
    );

  bleQueue =
    next.catch(
      () => {}
    );

  return next;
}


// ======================================================
// RUNTIME FORMAT
// ======================================================

function formatRuntime(
  totalSeconds
) {
  const safe =
    Math.max(
      0,
      Math.floor(
        totalSeconds
      )
    );

  const hours =
    Math.floor(
      safe / 3600
    );

  const minutes =
    Math.floor(
      (safe % 3600) /
      60
    );

  const seconds =
    safe % 60;


  return [
    hours,
    minutes,
    seconds
  ]
    .map(
      value =>
        String(value)
          .padStart(
            2,
            "0"
          )
    )
    .join(":");
}


// ======================================================
// RUNTIME UI
// ======================================================

function renderRuntimeClock() {
  let seconds =
    runtimeServerSeconds;


  if (
    runtimeActiveUI
  ) {
    seconds +=
      Math.floor(
        (
          Date.now() -
          runtimeLastSyncTime
        )
        /
        1000
      );
  }


  runtimeElapsed.textContent =
    formatRuntime(
      seconds
    );
}


function updateRuntimeUI(
  active,
  seconds,
  startPercent,
  startMilliVolts,
  source
) {
  runtimeActiveUI =
    Boolean(
      active
    );


  runtimeServerSeconds =
    Number.isFinite(
      seconds
    )
      ? seconds
      : 0;


  runtimeLastSyncTime =
    Date.now();


  runtimeState.classList.remove(
    "running",
    "paused"
  );


  if (
    runtimeActiveUI
  ) {
    runtimeState.textContent =
      "Running";

    runtimeState.classList.add(
      "running"
    );

  } else if (
    source === "USB"
  ) {
    runtimeState.textContent =
      "Paused";

    runtimeState.classList.add(
      "paused"
    );

  } else {
    runtimeState.textContent =
      "Waiting";
  }


  if (
    Number.isFinite(
      startPercent
    ) &&
    startPercent >= 0 &&
    Number.isFinite(
      startMilliVolts
    ) &&
    startMilliVolts > 0
  ) {
    runtimeStart.textContent =
      `Started at ${startPercent}% • ${(
        startMilliVolts /
        1000
      ).toFixed(2)} V`;

  } else {
    runtimeStart.textContent =
      source === "USB"
        ? "Unplug USB to start a battery session"
        : "Runtime baseline unavailable";
  }


  renderRuntimeClock();
}


// ======================================================
// RUNTIME CLOCK
// ======================================================

function startRuntimeClock() {
  if (
    runtimeClockTimer
  ) {
    return;
  }


  runtimeClockTimer =
    setInterval(
      renderRuntimeClock,
      1000
    );
}


// ======================================================
// RESET BATTERY STYLE
// ======================================================

function resetBatteryClasses() {
  batteryFill.classList.remove(
    "medium",
    "low",
    "critical",
    "external"
  );

  batteryStatus.classList.remove(
    "normal",
    "warning",
    "low",
    "critical",
    "external"
  );

  batteryCard.classList.remove(
    "warning",
    "critical",
    "external"
  );

  batteryWarning.classList.remove(
    "critical"
  );
}


// ======================================================
// WARNING
// ======================================================

function hideBatteryWarning() {
  batteryWarning.classList.add(
    "hidden"
  );
}


function showBatteryWarning(
  level
) {
  batteryWarning.classList.remove(
    "hidden"
  );

  batteryWarning.classList.remove(
    "critical"
  );


  if (
    level === "warning"
  ) {
    batteryWarningTitle.textContent =
      "Low Battery";

    batteryWarningText.textContent =
      "Battery is below 20%. Consider charging the HairClip soon.";

    return;
  }


  if (
    level === "low"
  ) {
    batteryWarningTitle.textContent =
      "Very Low Battery";

    batteryWarningText.textContent =
      "Battery is below 10%. Charge the HairClip when possible.";

    batteryWarning.classList.add(
      "critical"
    );

    return;
  }


  if (
    level === "critical"
  ) {
    batteryWarningTitle.textContent =
      "Charge Now";

    batteryWarningText.textContent =
      "Battery is at 5% or lower. Stop the runtime test and recharge the LiPo.";

    batteryWarning.classList.add(
      "critical"
    );
  }
}


// ======================================================
// POWER UI
// ======================================================

function updatePowerUI(
  source,
  percent,
  batteryMilliVolts,
  senseMilliVolts
) {
  resetBatteryClasses();

  hideBatteryWarning();


  if (
    source === "USB"
  ) {
    batteryPercentElement.textContent =
      "USB Power";

    batteryVoltageElement.textContent =
      "External";

    batteryFill.style.width =
      "100%";

    batteryFill.classList.add(
      "external"
    );

    batteryStatus.textContent =
      "External Power";

    batteryStatus.classList.add(
      "external"
    );

    batteryStatusIcon.textContent =
      "⚡";

    batteryCard.classList.add(
      "external"
    );

    batteryNote.textContent =
      "External power detected — battery percentage unavailable while USB is connected";

    return;
  }


  if (
    source === "BAT" &&
    Number.isFinite(percent) &&
    percent >= 0
  ) {
    const safePercent =
      Math.max(
        0,
        Math.min(
          100,
          percent
        )
      );


    batteryPercentElement.textContent =
      `${safePercent}%`;


    batteryFill.style.width =
      `${safePercent}%`;


    if (
      Number.isFinite(
        batteryMilliVolts
      ) &&
      batteryMilliVolts > 0
    ) {
      batteryVoltageElement.textContent =
        `${(
          batteryMilliVolts /
          1000
        ).toFixed(2)} V`;

    } else {
      batteryVoltageElement.textContent =
        "--.-- V";
    }


    batteryNote.textContent =
      "Estimated from LiPo voltage";


    if (
      safePercent > 20
    ) {
      batteryStatus.textContent =
        "Normal";

      batteryStatus.classList.add(
        "normal"
      );

      batteryStatusIcon.textContent =
        "●";

      return;
    }


    if (
      safePercent > 10
    ) {
      batteryFill.classList.add(
        "medium"
      );

      batteryStatus.textContent =
        "Low Battery";

      batteryStatus.classList.add(
        "warning"
      );

      batteryStatusIcon.textContent =
        "!";

      batteryCard.classList.add(
        "warning"
      );

      showBatteryWarning(
        "warning"
      );

      return;
    }


    if (
      safePercent > 5
    ) {
      batteryFill.classList.add(
        "low"
      );

      batteryStatus.textContent =
        "Very Low";

      batteryStatus.classList.add(
        "low"
      );

      batteryStatusIcon.textContent =
        "!!";

      batteryCard.classList.add(
        "critical"
      );

      showBatteryWarning(
        "low"
      );

      return;
    }


    batteryFill.classList.add(
      "critical"
    );

    batteryStatus.textContent =
      "Charge Now";

    batteryStatus.classList.add(
      "critical"
    );

    batteryStatusIcon.textContent =
      "!!!";

    batteryCard.classList.add(
      "critical"
    );

    showBatteryWarning(
      "critical"
    );

    return;
  }


  batteryPercentElement.textContent =
    "--%";

  batteryVoltageElement.textContent =
    "--.-- V";

  batteryFill.style.width =
    "0%";

  batteryStatus.textContent =
    "Unknown";

  batteryStatusIcon.textContent =
    "?";

  batteryNote.textContent =
    "Waiting for HairClip";
}


// ======================================================
// CONTROLS
// ======================================================

function setControlsEnabled(
  enabled
) {
  messageInput.disabled =
    !enabled;

  sendButton.disabled =
    !enabled;

  scrollToggle.disabled =
    !enabled;

  displayToggle.disabled =
    !enabled;

  runtimeResetButton.disabled =
    !enabled;

  iconButtons.forEach(
    button => {
      button.disabled =
        !enabled;
    }
  );
}


// ======================================================
// CONNECTION UI
// ======================================================

function setConnectionStatus(
  connected
) {
  if (
    connected
  ) {
    statusText.textContent =
      "Connected";

    statusBadge.classList.remove(
      "disconnected"
    );

    statusBadge.classList.add(
      "connected"
    );

    connectButton.textContent =
      "Disconnect";

    setControlsEnabled(
      true
    );

    return;
  }


  statusText.textContent =
    "Disconnected";

  statusBadge.classList.remove(
    "connected"
  );

  statusBadge.classList.add(
    "disconnected"
  );

  connectButton.textContent =
    device
      ? "Reconnect HairClip"
      : "Connect HairClip";

  setControlsEnabled(
    false
  );


  updatePowerUI(
    "UNK",
    null,
    null,
    null
  );


  updateRuntimeUI(
    false,
    0,
    -1,
    0,
    "UNK"
  );
}


// ======================================================
// CLEAR CONNECTION
// ======================================================

function clearConnection() {
  characteristic =
    null;

  stopAutoRefresh();

  setConnectionStatus(
    false
  );
}


// ======================================================
// READ STATE
// ======================================================

async function readControllerStateDirect({
  updateMessage = true,
  silent = false
} = {}) {
  if (
    !characteristic
  ) {
    return null;
  }


  try {
    const value =
      await characteristic.readValue();


    const response =
      new TextDecoder().decode(
        value
      );


    const newlineIndex =
      response.indexOf("\n");


    if (
      newlineIndex === -1
    ) {
      throw new Error(
        "Invalid HairClip response"
      );
    }


    const stateLine =
      response
        .slice(
          0,
          newlineIndex
        )
        .trim();


    const currentMessage =
      response
        .slice(
          newlineIndex + 1
        )
        .trim();


    const parts =
      stateLine.split(",");


    const scrollEnabled =
      parts[0] === "1";


    const displayEnabled =
      parts[1] === "1";


    let source =
      "UNK";

    let batteryPercent =
      null;

    let batteryMilliVolts =
      null;

    let senseMilliVolts =
      null;

    let runtimeActive =
      false;

    let runtimeSeconds =
      0;

    let runtimeStartPercent =
      -1;

    let runtimeStartMilliVolts =
      0;


    // V2.9+
    if (
      parts.length >= 10
    ) {
      source =
        parts[2];

      batteryPercent =
        Number(parts[3]);

      batteryMilliVolts =
        Number(parts[4]);

      senseMilliVolts =
        Number(parts[5]);

      runtimeActive =
        parts[6] === "1";

      runtimeSeconds =
        Number(parts[7]);

      runtimeStartPercent =
        Number(parts[8]);

      runtimeStartMilliVolts =
        Number(parts[9]);
    }

    // V2.8.2 compatibility
    else if (
      parts.length >= 6
    ) {
      source =
        parts[2];

      batteryPercent =
        Number(parts[3]);

      batteryMilliVolts =
        Number(parts[4]);

      senseMilliVolts =
        Number(parts[5]);
    }


    scrollToggle.checked =
      scrollEnabled;


    displayToggle.checked =
      displayEnabled;


    updatePowerUI(
      source,
      batteryPercent,
      batteryMilliVolts,
      senseMilliVolts
    );


    updateRuntimeUI(
      runtimeActive,
      runtimeSeconds,
      runtimeStartPercent,
      runtimeStartMilliVolts,
      source
    );


    if (
      updateMessage
    ) {
      messageInput.value =
        currentMessage;

      updateCharacterCount();
    }


    return {
      scrollEnabled,
      displayEnabled,
      source,
      batteryPercent,
      batteryMilliVolts,
      senseMilliVolts,
      runtimeActive,
      runtimeSeconds,
      runtimeStartPercent,
      runtimeStartMilliVolts,
      message:
        currentMessage
    };


  } catch (error) {
    console.error(
      "State sync failed:",
      error
    );


    if (
      !device ||
      !device.gatt ||
      !device.gatt.connected
    ) {
      clearConnection();

      log.textContent =
        "Connection lost";

    } else if (
      !silent
    ) {
      log.textContent =
        `State sync failed: ${error.message}`;
    }


    return null;
  }
}


// ======================================================
// QUEUED STATE READ
// ======================================================

function syncControllerState(
  options = {}
) {
  return enqueueBleOperation(
    () =>
      readControllerStateDirect(
        options
      )
  );
}


// ======================================================
// AUTO REFRESH
// ======================================================

function startAutoRefresh() {
  stopAutoRefresh();


  autoRefreshTimer =
    setInterval(
      async () => {
        if (
          !isConnected()
        ) {
          return;
        }


        await syncControllerState({
          updateMessage: false,
          silent: true
        });
      },

      AUTO_REFRESH_INTERVAL
    );
}


function stopAutoRefresh() {
  if (
    autoRefreshTimer
  ) {
    clearInterval(
      autoRefreshTimer
    );

    autoRefreshTimer =
      null;
  }
}


// ======================================================
// CONNECT DEVICE
// ======================================================

async function connectToDevice() {
  if (
    !device
  ) {
    throw new Error(
      "No Bluetooth device selected"
    );
  }


  log.textContent =
    `Connecting to ${
      device.name ||
      "HairClip"
    }...`;


  let server;


  if (
    device.gatt.connected
  ) {
    server =
      device.gatt;

  } else {
    server =
      await device.gatt.connect();
  }


  const service =
    await server.getPrimaryService(
      SERVICE_UUID
    );


  characteristic =
    await service.getCharacteristic(
      CHARACTERISTIC_UUID
    );


  setConnectionStatus(
    true
  );


  const state =
    await syncControllerState({
      updateMessage: true
    });


  if (
    !state
  ) {
    throw new Error(
      "State sync failed"
    );
  }


  startAutoRefresh();


  log.textContent =
    `Connected to ${
      device.name ||
      "HairClip"
    }`;
}


// ======================================================
// SELECT DEVICE
// ======================================================

async function selectHairClip() {
  const selectedDevice =
    await navigator.bluetooth.requestDevice({
      filters: [
        {
          name:
            "HairClip-V1"
        }
      ],

      optionalServices: [
        SERVICE_UUID
      ]
    });


  device =
    selectedDevice;


  device.addEventListener(
    "gattserverdisconnected",
    handleDisconnected
  );
}


// ======================================================
// CONNECT
// ======================================================

async function connectHairClip() {
  if (
    isConnecting
  ) {
    return;
  }


  isConnecting =
    true;

  connectButton.disabled =
    true;

  manualDisconnect =
    false;


  try {
    if (
      !device
    ) {
      await selectHairClip();
    }


    await connectToDevice();


  } catch (error) {
    console.error(
      error
    );

    characteristic =
      null;

    stopAutoRefresh();

    setConnectionStatus(
      false
    );

    log.textContent =
      `Connection failed: ${error.message}`;


  } finally {
    isConnecting =
      false;

    connectButton.disabled =
      false;
  }
}


// ======================================================
// DISCONNECT
// ======================================================

function disconnectHairClip() {
  manualDisconnect =
    true;

  stopAutoRefresh();


  if (
    device &&
    device.gatt &&
    device.gatt.connected
  ) {
    device.gatt.disconnect();
  }
}


// ======================================================
// DISCONNECT EVENT
// ======================================================

function handleDisconnected() {
  characteristic =
    null;

  stopAutoRefresh();

  setConnectionStatus(
    false
  );


  log.textContent =
    manualDisconnect
      ? "HairClip disconnected"
      : "Connection lost — tap Reconnect HairClip";


  manualDisconnect =
    false;
}


// ======================================================
// WRITE
// ======================================================

async function sendCommandDirect(
  command
) {
  if (
    !isConnected()
  ) {
    return false;
  }


  try {
    const encoder =
      new TextEncoder();


    await characteristic.writeValue(
      encoder.encode(
        command
      )
    );


    return true;


  } catch (error) {
    console.error(
      error
    );

    return false;
  }
}


// ======================================================
// SEND + SYNC
// ======================================================

function sendAndSync(
  command,
  {
    updateMessage = false
  } = {}
) {
  return enqueueBleOperation(
    async () => {
      const success =
        await sendCommandDirect(
          command
        );


      if (
        !success
      ) {
        return null;
      }


      await wait(
        100
      );


      return await readControllerStateDirect({
        updateMessage,
        silent: false
      });
    }
  );
}


// ======================================================
// RESET RUNTIME
// ======================================================

runtimeResetButton.addEventListener(
  "click",
  async () => {
    runtimeResetButton.disabled =
      true;


    log.textContent =
      "Resetting runtime test...";


    const state =
      await sendAndSync(
        "RUNTIME:RESET",
        {
          updateMessage: false
        }
      );


    if (
      state
    ) {
      log.textContent =
        state.source === "BAT"
          ? "Runtime test restarted"
          : "Runtime reset — unplug USB to start";
    }


    if (
      isConnected()
    ) {
      runtimeResetButton.disabled =
        false;
    }
  }
);


// ======================================================
// ICONS
// ======================================================

function insertIconToken(
  token
) {
  const value =
    messageInput.value;


  const start =
    messageInput.selectionStart ??
    value.length;


  const end =
    messageInput.selectionEnd ??
    value.length;


  const before =
    value.slice(
      0,
      start
    );


  const after =
    value.slice(
      end
    );


  let inserted =
    token;


  if (
    before.length &&
    !before.endsWith(" ")
  ) {
    inserted =
      " " +
      inserted;
  }


  if (
    after.length &&
    !after.startsWith(" ")
  ) {
    inserted +=
      " ";
  }


  const result =
    before +
    inserted +
    after;


  if (
    result.length > 100
  ) {
    return;
  }


  messageInput.value =
    result;


  updateCharacterCount();


  const cursor =
    before.length +
    inserted.length;


  messageInput.focus();


  messageInput.setSelectionRange(
    cursor,
    cursor
  );
}


iconButtons.forEach(
  button => {
    button.addEventListener(
      "click",
      () =>
        insertIconToken(
          button.dataset.token
        )
    );
  }
);


// ======================================================
// CONNECT BUTTON
// ======================================================

connectButton.addEventListener(
  "click",
  async () => {
    if (
      device?.gatt?.connected
    ) {
      disconnectHairClip();

    } else {
      await connectHairClip();
    }
  }
);


// ======================================================
// INPUT
// ======================================================

messageInput.addEventListener(
  "input",
  updateCharacterCount
);


// ======================================================
// SEND MESSAGE
// ======================================================

sendButton.addEventListener(
  "click",
  async () => {
    const text =
      messageInput
        .value
        .trim();


    if (
      !text
    ) {
      log.textContent =
        "Type a message first";

      return;
    }


    sendButton.disabled =
      true;


    const state =
      await sendAndSync(
        `TEXT:${text}`,
        {
          updateMessage: true
        }
      );


    if (
      state
    ) {
      log.textContent =
        "Message updated";
    }


    if (
      isConnected()
    ) {
      sendButton.disabled =
        false;
    }
  }
);


// ======================================================
// ENTER
// ======================================================

messageInput.addEventListener(
  "keydown",
  event => {
    if (
      event.key === "Enter"
    ) {
      event.preventDefault();

      sendButton.click();
    }
  }
);


// ======================================================
// SCROLL
// ======================================================

scrollToggle.addEventListener(
  "change",
  async () => {
    scrollToggle.disabled =
      true;


    const requested =
      scrollToggle.checked;


    const state =
      await sendAndSync(
        requested
          ? "SCROLL:ON"
          : "SCROLL:OFF"
      );


    if (
      state &&
      !requested &&
      state.scrollEnabled
    ) {
      log.textContent =
        "Message is too long — scrolling stays ON";
    }


    if (
      isConnected()
    ) {
      scrollToggle.disabled =
        false;
    }
  }
);


// ======================================================
// DISPLAY
// ======================================================

displayToggle.addEventListener(
  "change",
  async () => {
    displayToggle.disabled =
      true;


    await sendAndSync(
      displayToggle.checked
        ? "DISPLAY:ON"
        : "DISPLAY:OFF"
    );


    if (
      isConnected()
    ) {
      displayToggle.disabled =
        false;
    }
  }
);


// ======================================================
// INITIAL
// ======================================================

startRuntimeClock();


updatePowerUI(
  "UNK",
  null,
  null,
  null
);


updateRuntimeUI(
  false,
  0,
  -1,
  0,
  "UNK"
);


setConnectionStatus(
  false
);