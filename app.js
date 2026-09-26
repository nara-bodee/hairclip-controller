// ======================================================
// BLE CONFIG
// ======================================================

const SERVICE_UUID =
  "c7a10001-6c9e-4d5d-a001-123456789abc";

const CHARACTERISTIC_UUID =
  "c7a10002-6c9e-4d5d-a001-123456789abc";


// ======================================================
// BLE STATE
// ======================================================

let device = null;
let characteristic = null;

let isConnecting = false;
let manualDisconnect = false;


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
}


// ======================================================
// CONNECTION UI
// ======================================================

function setConnectionStatus(
  connected
) {
  if (connected) {
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
}


function clearConnection() {
  characteristic =
    null;

  setConnectionStatus(
    false
  );
}


// ======================================================
// SYNC STATE
// ======================================================

async function syncControllerState() {
  if (!characteristic) {
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

    // รูปแบบ:
    //
    // 1,1
    // HELLO WORLD

    const newlineIndex =
      response.indexOf("\n");

    if (
      newlineIndex === -1
    ) {
      throw new Error(
        "Invalid HairClip response"
      );
    }


    const state =
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
      state.split(",");


    if (
      parts.length !== 2
    ) {
      throw new Error(
        "Invalid state response"
      );
    }


    const scrollEnabled =
      parts[0] === "1";


    const displayEnabled =
      parts[1] === "1";


    // Update UI
    scrollToggle.checked =
      scrollEnabled;

    displayToggle.checked =
      displayEnabled;

    messageInput.value =
      currentMessage;

    characterCount.textContent =
      `${currentMessage.length} / 100`;


    return {
      scrollEnabled,
      displayEnabled,
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


    return null;
  }
}


// ======================================================
// CONNECT TO DEVICE
// ======================================================

async function connectToDevice() {
  if (!device) {
    throw new Error(
      "No Bluetooth device selected"
    );
  }


  log.textContent =
    `Connecting to ${
      device.name || "HairClip"
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
    await syncControllerState();


  if (!state) {
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


  log.textContent =
    `Connected to ${
      device.name || "HairClip"
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
  if (isConnecting) {
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
    if (!device) {
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

  setConnectionStatus(
    false
  );


  if (manualDisconnect) {
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
// SEND COMMAND
// ======================================================

async function sendCommand(
  command
) {
  if (!isConnected()) {
    clearConnection();

    log.textContent =
      "HairClip is not connected";

    return false;
  }


  try {
    const encoder =
      new TextEncoder();

    const data =
      encoder.encode(
        command
      );


    await characteristic.writeValue(
      data
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
// SEND + WAIT + READ REAL STATE
// ======================================================

async function sendAndSync(
  command
) {
  const success =
    await sendCommand(
      command
    );


  if (!success) {
    return null;
  }


  // Firmware รับ command ใน BLE callback
  // แล้ว main loop ค่อย execute
  // จึงรอสั้น ๆ ก่อน Read state
  await wait(
    100
  );


  return await syncControllerState();
}


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
// CHARACTER COUNT
// ======================================================

messageInput.addEventListener(
  "input",
  () => {
    characterCount.textContent =
      `${messageInput.value.length} / 100`;
  }
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


    if (!text) {
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
        `TEXT:${text}`
      );


    if (state) {
      sendButton.textContent =
        "Sent ✓";


      if (
        state.scrollEnabled
      ) {
        log.textContent =
          "Message updated";
      } else {
        log.textContent =
          "Message updated — scrolling is OFF";
      }


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
      event.key === "Enter"
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
          : "SCROLL:OFF"
      );


    if (state) {
      // syncControllerState()
      // ตั้ง Toggle ตามค่าจริงจาก ESP32 แล้ว


      if (
        !requestedState &&
        state.scrollEnabled
      ) {
        // ผู้ใช้ขอ OFF
        // แต่ ESP32 ยังคง ON
        log.textContent =
          "Message is too long — scrolling stays ON";
      }

      else if (
        requestedState &&
        state.scrollEnabled
      ) {
        log.textContent =
          "Scrolling ON";
      }

      else {
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
          : "DISPLAY:OFF"
      );


    if (state) {
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
// INITIAL UI
// ======================================================

setConnectionStatus(
  false
);