// ======================================================
// BLE CONFIG
// ======================================================

const SERVICE_UUID =
  "c7a10001-6c9e-4d5d-a001-123456789abc";

const CHARACTERISTIC_UUID =
  "c7a10002-6c9e-4d5d-a001-123456789abc";


// หน้าเว็บอ่าน State ทุก 30 วินาที
const AUTO_REFRESH_INTERVAL =
  30000;


// ======================================================
// BLE STATE
// ======================================================

let device =
  null;

let characteristic =
  null;


let isConnecting =
  false;

let manualDisconnect =
  false;


let autoRefreshTimer =
  null;


let bleQueue =
  Promise.resolve();


// ======================================================
// RUNTIME UI
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


// ======================================================
// BATTERY
// ======================================================

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


// ======================================================
// DIAGNOSTICS
// ======================================================

const diagFirmware =
  document.getElementById(
    "diagFirmware"
  );


const diagBle =
  document.getElementById(
    "diagBle"
  );


const diagPowerSource =
  document.getElementById(
    "diagPowerSource"
  );


const diagSafety =
  document.getElementById(
    "diagSafety"
  );


const diagSense =
  document.getElementById(
    "diagSense"
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


// ------------------------------------------------------

function isConnected() {

  return Boolean(
    device &&
    device.gatt &&
    device.gatt.connected &&
    characteristic
  );
}


// ------------------------------------------------------

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
// TIME FORMAT
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
      safe /
      3600
    );


  const minutes =
    Math.floor(
      (
        safe %
        3600
      )
      /
      60
    );


  const seconds =
    safe %
    60;


  return [
    hours,
    minutes,
    seconds
  ]
    .map(
      value =>
        String(
          value
        ).padStart(
          2,
          "0"
        )
    )
    .join(
      ":"
    );
}


// ======================================================
// RUNTIME CLOCK
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
          Date.now()
          -
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


// ------------------------------------------------------

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


  if (
    runtimeActiveUI
  ) {

    runtimeState.textContent =
      "Running";

  } else if (
    source === "USB"
  ) {

    runtimeState.textContent =
      "Paused";

  } else {

    runtimeState.textContent =
      "Waiting";
  }


  if (
    Number.isFinite(
      startPercent
    )
    &&
    startPercent >= 0
    &&
    Number.isFinite(
      startMilliVolts
    )
    &&
    startMilliVolts > 0
  ) {

    runtimeStart.textContent =
      `Started at ${startPercent}% • ${(
        startMilliVolts /
        1000
      ).toFixed(2)} V`;

  } else if (
    source === "USB"
  ) {

    runtimeStart.textContent =
      "Unplug USB to start a battery session";

  } else {

    runtimeStart.textContent =
      "Runtime baseline unavailable";
  }


  renderRuntimeClock();
}


// ------------------------------------------------------

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
// BATTERY UI RESET
// ======================================================

function resetBatteryClasses() {

  batteryFill.classList.remove(
    "low",
    "critical",
    "external"
  );


  batteryStatus.classList.remove(
    "normal",
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
// BATTERY WARNING
// ======================================================

function hideBatteryWarning() {

  batteryWarning.classList.add(
    "hidden"
  );
}


// ------------------------------------------------------

function showBatteryWarning(
  safety
) {

  batteryWarning.classList.remove(
    "hidden"
  );


  batteryWarning.classList.remove(
    "critical"
  );


  if (
    safety === "LOW"
  ) {

    batteryWarningTitle.textContent =
      "Low Battery";


    batteryWarningText.textContent =
      "Battery voltage is low. Charge the HairClip soon.";


    return;
  }


  if (
    safety === "CRITICAL"
  ) {

    batteryWarningTitle.textContent =
      "Critical Battery";


    batteryWarningText.textContent =
      "Battery voltage is critically low. Charge the HairClip now.";


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
  senseMilliVolts,
  safety
) {

  resetBatteryClasses();

  hideBatteryWarning();


  // ==================================================
  // USB
  // ==================================================

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


  // ==================================================
  // BATTERY
  // ==================================================

  if (
    source === "BAT"
    &&
    Number.isFinite(
      percent
    )
    &&
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
      )
      &&
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


    // ================================================
    // CRITICAL
    // ================================================

    if (
      safety === "CRITICAL"
    ) {

      batteryFill.classList.add(
        "critical"
      );


      batteryStatus.textContent =
        "Critical Battery";


      batteryStatus.classList.add(
        "critical"
      );


      batteryStatusIcon.textContent =
        "!!";


      batteryCard.classList.add(
        "critical"
      );


      showBatteryWarning(
        "CRITICAL"
      );


      return;
    }


    // ================================================
    // LOW
    // ================================================

    if (
      safety === "LOW"
    ) {

      batteryFill.classList.add(
        "low"
      );


      batteryStatus.textContent =
        "Low Battery";


      batteryStatus.classList.add(
        "low"
      );


      batteryStatusIcon.textContent =
        "!";


      batteryCard.classList.add(
        "warning"
      );


      showBatteryWarning(
        "LOW"
      );


      return;
    }


    // ================================================
    // NORMAL
    // ================================================

    batteryStatus.textContent =
      "Normal";


    batteryStatus.classList.add(
      "normal"
    );


    batteryStatusIcon.textContent =
      "●";


    return;
  }


  // ==================================================
  // UNKNOWN
  // ==================================================

  batteryPercentElement.textContent =
    "--%";


  batteryVoltageElement.textContent =
    Number.isFinite(
      senseMilliVolts
    )
    &&
    senseMilliVolts > 0

      ? `${(
          senseMilliVolts /
          1000
        ).toFixed(2)} V`

      : "--.-- V";


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
// DIAGNOSTICS UI
// ======================================================

function updateDiagnostics({
  firmwareVersion = "—",
  source = "UNK",
  safety = "NA",
  senseMilliVolts = null
} = {}) {

  diagFirmware.textContent =
    firmwareVersion
      ? `v${firmwareVersion}`
      : "—";


  diagBle.textContent =
    isConnected()
      ? "Connected"
      : "Disconnected";


  if (
    source === "BAT"
  ) {

    diagPowerSource.textContent =
      "Battery";

  } else if (
    source === "USB"
  ) {

    diagPowerSource.textContent =
      "USB / External";

  } else {

    diagPowerSource.textContent =
      "Unknown";
  }


  diagSafety.textContent =
    source === "BAT"
      ? safety
      : "N/A";


  if (
    Number.isFinite(
      senseMilliVolts
    )
    &&
    senseMilliVolts > 0
  ) {

    diagSense.textContent =
      `${(
        senseMilliVolts /
        1000
      ).toFixed(3)} V`;

  } else {

    diagSense.textContent =
      "—";
  }
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


    diagBle.textContent =
      "Connected";


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


  diagBle.textContent =
    "Disconnected";


  setControlsEnabled(
    false
  );


  updatePowerUI(
    "UNK",
    null,
    null,
    null,
    "NA"
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
      response.indexOf(
        "\n"
      );


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
      stateLine.split(
        ","
      );


    if (
      parts.length < 2
    ) {

      throw new Error(
        "Invalid state response"
      );
    }


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


    let safety =
      "NA";


    let firmwareVersion =
      "Legacy";


    // ==================================================
    // V2.12
    // ==================================================

    if (
      parts.length >= 12
    ) {

      source =
        parts[2];


      batteryPercent =
        Number(
          parts[3]
        );


      batteryMilliVolts =
        Number(
          parts[4]
        );


      senseMilliVolts =
        Number(
          parts[5]
        );


      runtimeActive =
        parts[6] === "1";


      runtimeSeconds =
        Number(
          parts[7]
        );


      runtimeStartPercent =
        Number(
          parts[8]
        );


      runtimeStartMilliVolts =
        Number(
          parts[9]
        );


      safety =
        parts[10];


      firmwareVersion =
        parts[11];
    }


    // ==================================================
    // V2.9 / V2.10 compatibility
    // ==================================================

    else if (
      parts.length >= 10
    ) {

      source =
        parts[2];


      batteryPercent =
        Number(
          parts[3]
        );


      batteryMilliVolts =
        Number(
          parts[4]
        );


      senseMilliVolts =
        Number(
          parts[5]
        );


      runtimeActive =
        parts[6] === "1";


      runtimeSeconds =
        Number(
          parts[7]
        );


      runtimeStartPercent =
        Number(
          parts[8]
        );


      runtimeStartMilliVolts =
        Number(
          parts[9]
        );


      safety =
        "NA";
    }


    // ==================================================
    // V2.8.2 compatibility
    // ==================================================

    else if (
      parts.length >= 6
    ) {

      source =
        parts[2];


      batteryPercent =
        Number(
          parts[3]
        );


      batteryMilliVolts =
        Number(
          parts[4]
        );


      senseMilliVolts =
        Number(
          parts[5]
        );
    }


    scrollToggle.checked =
      scrollEnabled;


    displayToggle.checked =
      displayEnabled;


    updatePowerUI(
      source,
      batteryPercent,
      batteryMilliVolts,
      senseMilliVolts,
      safety
    );


    updateRuntimeUI(
      runtimeActive,
      runtimeSeconds,
      runtimeStartPercent,
      runtimeStartMilliVolts,
      source
    );


    updateDiagnostics({
      firmwareVersion,
      source,
      safety,
      senseMilliVolts
    });


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

      safety,

      firmwareVersion,

      message:
        currentMessage
    };


  } catch (error) {

    console.error(
      "State sync failed:",
      error
    );


    if (
      !device
      ||
      !device.gatt
      ||
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
// QUEUED READ
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

          updateMessage:
            false,

          silent:
            true
        });
      },

      AUTO_REFRESH_INTERVAL
    );
}


// ------------------------------------------------------

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


  log.textContent =
    "Synchronizing state...";


  const state =
    await syncControllerState({
      updateMessage: true
    });


  if (
    !state
  ) {

    throw new Error(
      "State synchronization failed"
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

  log.textContent =
    "Searching for HairClip...";


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


  if (
    !navigator.bluetooth
  ) {

    log.textContent =
      "Web Bluetooth is not supported in this browser";


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
      "Connection failed:",
      error
    );


    characteristic =
      null;


    stopAutoRefresh();


    setConnectionStatus(
      false
    );


    if (
      error.name ===
      "NotFoundError"
    ) {

      log.textContent =
        "Bluetooth selection cancelled";

    } else {

      log.textContent =
        `Connection failed: ${error.message}`;
    }


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
    device
    &&
    device.gatt
    &&
    device.gatt.connected
  ) {

    device.gatt.disconnect();

  } else {

    clearConnection();


    log.textContent =
      "HairClip disconnected";
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

    clearConnection();


    log.textContent =
      "HairClip is not connected";


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
      "BLE write failed:",
      error
    );


    if (
      !device?.gatt?.connected
    ) {

      clearConnection();
    }


    log.textContent =
      `BLE error: ${error.message}`;


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

        silent:
          false
      });
    }
  );
}


// ======================================================
// ICON INSERTION
// ======================================================

function insertIconToken(
  token
) {

  if (
    messageInput.disabled
  ) {

    return;
  }


  const value =
    messageInput.value;


  const start =
    messageInput.selectionStart
    ??
    value.length;


  const end =
    messageInput.selectionEnd
    ??
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
    before.length > 0
    &&
    !before.endsWith(
      " "
    )
  ) {

    inserted =
      " "
      +
      inserted;
  }


  if (
    after.length > 0
    &&
    !after.startsWith(
      " "
    )
  ) {

    inserted +=
      " ";
  }


  const result =
    before
    +
    inserted
    +
    after;


  if (
    result.length >
    100
  ) {

    log.textContent =
      "Message limit reached";


    return;
  }


  messageInput.value =
    result;


  updateCharacterCount();


  const cursor =
    before.length
    +
    inserted.length;


  messageInput.focus();


  messageInput.setSelectionRange(
    cursor,
    cursor
  );
}


// ------------------------------------------------------

iconButtons.forEach(
  button => {

    button.addEventListener(
      "click",
      () => {

        insertIconToken(
          button.dataset.token
        );
      }
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


    sendButton.textContent =
      "Sending...";


    const state =
      await sendAndSync(

        `TEXT:${text}`,

        {
          updateMessage:
            true
        }
      );


    if (
      state
    ) {

      sendButton.textContent =
        "Sent ✓";


      log.textContent =
        "Message updated";
    }


    setTimeout(

      () => {

        sendButton.textContent =
          "Send Message";


        if (
          isConnected()
        ) {

          sendButton.disabled =
            false;
        }
      },

      800
    );
  }
);


// ======================================================
// ENTER
// ======================================================

messageInput.addEventListener(

  "keydown",

  event => {

    if (
      event.key ===
      "Enter"
    ) {

      event.preventDefault();


      if (
        !sendButton.disabled
      ) {

        sendButton.click();
      }
    }
  }
);


// ======================================================
// SCROLL
// ======================================================

scrollToggle.addEventListener(

  "change",

  async () => {

    const requested =
      scrollToggle.checked;


    scrollToggle.disabled =
      true;


    const state =
      await sendAndSync(

        requested
          ? "SCROLL:ON"
          : "SCROLL:OFF",

        {
          updateMessage:
            false
        }
      );


    if (
      state
    ) {

      if (
        !requested
        &&
        state.scrollEnabled
      ) {

        log.textContent =
          "Message is too long — scrolling stays ON";

      } else {

        log.textContent =
          state.scrollEnabled
            ? "Scrolling ON"
            : "Scrolling OFF";
      }
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


    const state =
      await sendAndSync(

        displayToggle.checked
          ? "DISPLAY:ON"
          : "DISPLAY:OFF",

        {
          updateMessage:
            false
        }
      );


    if (
      state
    ) {

      log.textContent =
        state.displayEnabled
          ? "Display ON"
          : "Display OFF";
    }


    if (
      isConnected()
    ) {

      displayToggle.disabled =
        false;
    }
  }
);


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
          updateMessage:
            false
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
// INITIAL
// ======================================================

startRuntimeClock();


updatePowerUI(
  "UNK",
  null,
  null,
  null,
  "NA"
);


updateRuntimeUI(
  false,
  0,
  -1,
  0,
  "UNK"
);


updateDiagnostics({
  firmwareVersion:
    "",
  source:
    "UNK",
  safety:
    "NA",
  senseMilliVolts:
    null
});


setConnectionStatus(
  false
);