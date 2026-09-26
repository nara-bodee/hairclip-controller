// ======================================================
// BLE CONFIG
// ======================================================

const SERVICE_UUID =
  "c7a10001-6c9e-4d5d-a001-123456789abc";

const CHARACTERISTIC_UUID =
  "c7a10002-6c9e-4d5d-a001-123456789abc";

const AUTO_REFRESH_INTERVAL =
  30000;


// ======================================================
// BLE STATE
// ======================================================

let device = null;
let characteristic = null;

let isConnecting = false;
let manualDisconnect = false;

let autoRefreshTimer = null;

// Serialize BLE operations.
// ป้องกัน Read/Write ชนกัน
let bleQueue =
  Promise.resolve();


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
// POWER UI
// ======================================================

function updatePowerUI(
  source,
  percent,
  batteryMilliVolts,
  senseMilliVolts
) {
  batteryFill.classList.remove(
    "medium",
    "low",
    "external"
  );


  // --------------------------------------------------
  // USB / EXTERNAL POWER
  // --------------------------------------------------

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

    batteryNote.textContent =
      "External power detected — battery % is unavailable while USB is connected";

    return;
  }


  // --------------------------------------------------
  // BATTERY
  // --------------------------------------------------

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


    batteryFill.style.width =
      `${safePercent}%`;


    if (
      safePercent <= 20
    ) {
      batteryFill.classList.add(
        "low"
      );

    } else if (
      safePercent <= 50
    ) {
      batteryFill.classList.add(
        "medium"
      );
    }


    batteryNote.textContent =
      "Estimated from LiPo voltage";

    return;
  }


  // --------------------------------------------------
  // UNKNOWN
  // --------------------------------------------------

  batteryPercentElement.textContent =
    "--%";

  batteryFill.style.width =
    "0%";


  if (
    Number.isFinite(
      senseMilliVolts
    ) &&
    senseMilliVolts > 0
  ) {
    batteryVoltageElement.textContent =
      `${(
        senseMilliVolts /
        1000
      ).toFixed(2)} V`;

    batteryNote.textContent =
      "Power source could not be classified";

  } else {
    batteryVoltageElement.textContent =
      "--.-- V";

    batteryNote.textContent =
      "Waiting for HairClip";
  }
}


// ======================================================
// CONTROL STATE
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
}


function clearConnection() {
  characteristic =
    null;

  stopAutoRefresh();

  setConnectionStatus(
    false
  );
}


// ======================================================
// DIRECT STATE READ
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


    console.log(
      "HairClip response:",
      response
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


    // ==================================================
    // V2.8.2
    //
    // 1,1,BAT,86,4090,4090
    // ==================================================

    if (
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


    // ==================================================
    // V2.8.1 compatibility
    //
    // 1,1,86,4090
    // ==================================================

    else if (
      parts.length >= 4
    ) {
      batteryPercent =
        Number(
          parts[2]
        );

      batteryMilliVolts =
        Number(
          parts[3]
        );

      source =
        (
          Number.isFinite(
            batteryPercent
          ) &&
          batteryPercent >= 0
        )
          ? "BAT"
          : "UNK";
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


    // Auto refresh จะไม่แตะ draft
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
    }

    else if (
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


        // ไม่ update ช่อง Message
        // เพื่อไม่ลบข้อความที่ผู้ใช้กำลังพิมพ์
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
// CONNECT TO DEVICE
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
    if (
      device.gatt.connected
    ) {
      log.textContent =
        "Connected, but state sync failed";

      return;
    }


    throw new Error(
      "Connection lost during synchronization"
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
// CONNECT / RECONNECT
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
    device &&
    device.gatt &&
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


  if (
    manualDisconnect
  ) {
    log.textContent =
      "HairClip disconnected";

  } else {
    log.textContent =
      "Connection lost — tap Reconnect HairClip";
  }


  manualDisconnect =
    false;
}


// ======================================================
// BLE ERROR
// ======================================================

function handleBleFailure(
  error
) {
  console.error(
    "BLE error:",
    error
  );


  if (
    !device ||
    !device.gatt ||
    !device.gatt.connected
  ) {
    characteristic =
      null;

    stopAutoRefresh();

    setConnectionStatus(
      false
    );

    log.textContent =
      "Connection lost — tap Reconnect HairClip";

    return;
  }


  log.textContent =
    `BLE error: ${error.message}`;
}


// ======================================================
// DIRECT WRITE
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
    handleBleFailure(
      error
    );

    return false;
  }
}


// ======================================================
// SEND + READ STATE
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


  const currentValue =
    messageInput.value;


  const start =
    messageInput.selectionStart ??
    currentValue.length;


  const end =
    messageInput.selectionEnd ??
    currentValue.length;


  const before =
    currentValue.slice(
      0,
      start
    );


  const after =
    currentValue.slice(
      end
    );


  const needsSpaceBefore =
    before.length > 0 &&
    !before.endsWith(" ");


  const needsSpaceAfter =
    after.length > 0 &&
    !after.startsWith(" ");


  let inserted =
    token;


  if (
    needsSpaceBefore
  ) {
    inserted =
      " " +
      inserted;
  }


  if (
    needsSpaceAfter
  ) {
    inserted =
      inserted +
      " ";
  }


  const newValue =
    before +
    inserted +
    after;


  if (
    newValue.length >
    100
  ) {
    log.textContent =
      "Message limit reached";

    return;
  }


  messageInput.value =
    newValue;


  updateCharacterCount();


  const newCursorPosition =
    before.length +
    inserted.length;


  messageInput.focus();


  messageInput.setSelectionRange(
    newCursorPosition,
    newCursorPosition
  );


  log.textContent =
    `Added ${token}`;
}


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
      device &&
      device.gatt &&
      device.gatt.connected
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
          updateMessage: true
        }
      );


    if (
      state
    ) {
      sendButton.textContent =
        "Sent ✓";


      log.textContent =
        state.scrollEnabled
          ? "Message updated"
          : "Message updated — scrolling is OFF";

    } else {
      sendButton.textContent =
        "Send Message";
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

      900
    );
  }
);


// ======================================================
// ENTER = SEND
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
    const requestedState =
      scrollToggle.checked;


    scrollToggle.disabled =
      true;


    log.textContent =
      requestedState
        ? "Turning scrolling on..."
        : "Turning scrolling off...";


    const state =
      await sendAndSync(
        requestedState
          ? "SCROLL:ON"
          : "SCROLL:OFF",
        {
          // สำคัญ:
          // อย่าเขียนทับข้อความที่กำลังพิมพ์
          updateMessage: false
        }
      );


    if (
      state
    ) {
      if (
        !requestedState &&
        state.scrollEnabled
      ) {
        log.textContent =
          "Message is too long — scrolling stays ON";

      } else if (
        state.scrollEnabled
      ) {
        log.textContent =
          "Scrolling ON";

      } else {
        log.textContent =
          "Scrolling OFF";
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
    const requestedState =
      displayToggle.checked;


    displayToggle.disabled =
      true;


    const state =
      await sendAndSync(
        requestedState
          ? "DISPLAY:ON"
          : "DISPLAY:OFF",
        {
          updateMessage: false
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
// INITIAL
// ======================================================

updatePowerUI(
  "UNK",
  null,
  null,
  null
);

setConnectionStatus(
  false
);