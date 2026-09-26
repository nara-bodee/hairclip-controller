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
// CONNECTION HELPERS
// ======================================================

function isConnected() {

  return Boolean(
    device &&
    device.gatt &&
    device.gatt.connected &&
    characteristic
  );
}


// ======================================================
// ENABLE / DISABLE CONTROLS
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
// CONNECTION STATUS UI
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

  } else {

    statusText.textContent =
      "Disconnected";


    statusBadge.classList.remove(
      "connected"
    );


    statusBadge.classList.add(
      "disconnected"
    );


    // ถ้า browser ยังจำ device object ได้
    // ครั้งต่อไป reconnect ได้โดยไม่เปิด chooser
    connectButton.textContent =
      device
        ? "Reconnect HairClip"
        : "Connect HairClip";


    setControlsEnabled(
      false
    );
  }
}


// ======================================================
// CLEAR ACTIVE CONNECTION
// ======================================================

function clearConnection() {

  characteristic =
    null;


  setConnectionStatus(
    false
  );
}


// ======================================================
// SYNC STATE FROM HAIRCLIP
// ======================================================

async function syncControllerState() {

  if (!characteristic) {

    return false;
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


    // ==================================================
    // STATE
    // ==================================================

    const parts =
      state.split(",");


    if (
      parts.length !== 2
    ) {

      throw new Error(
        "Invalid state response"
      );
    }


    scrollToggle.checked =
      parts[0] === "1";


    displayToggle.checked =
      parts[1] === "1";


    // ==================================================
    // MESSAGE
    // ==================================================

    messageInput.value =
      currentMessage;


    characterCount.textContent =
      `${currentMessage.length} / 100`;


    return true;


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


    return false;
  }
}


// ======================================================
// CONNECT TO CURRENT DEVICE
// ======================================================

async function connectToDevice() {

  if (!device) {

    throw new Error(
      "No Bluetooth device selected"
    );
  }


  log.textContent =
    `Connecting to ${device.name || "HairClip"}...`;


  let server;


  // ถ้า GATT ยัง connected อยู่
  // ไม่ต้อง connect ซ้ำ
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


  const synced =
    await syncControllerState();


  if (!synced) {

    // ถ้า BLE ยังต่ออยู่
    // แค่ State Sync มีปัญหา
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
    `Connected to ${device.name || "HairClip"}`;
}


// ======================================================
// SELECT NEW DEVICE
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

    // --------------------------------------------------
    // ถ้ายังไม่เคยเลือก HairClip ใน session นี้
    // เปิด Bluetooth chooser
    // --------------------------------------------------

    if (!device) {

      await selectHairClip();
    }


    // --------------------------------------------------
    // ถ้ามี device อยู่แล้ว
    // จะ reconnect ตัวเดิมทันที
    // ไม่ต้องเลือกใหม่
    // --------------------------------------------------

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
// GATT DISCONNECT EVENT
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
// HANDLE BLE FAILURE
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

  // --------------------------------------------------
  // ตรวจ Connection ก่อนส่ง
  // --------------------------------------------------

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


    log.textContent =
      `Sent: ${command}`;


    return true;


  } catch (error) {

    handleBleFailure(
      error
    );


    return false;
  }
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
// MESSAGE CHARACTER COUNT
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


    // --------------------------------------------------
    // UI: Sending
    // --------------------------------------------------

    sendButton.disabled =
      true;


    sendButton.textContent =
      "Sending...";


    // --------------------------------------------------
    // BLE
    // --------------------------------------------------

    const success =
      await sendCommand(
        `TEXT:${text}`
      );


    // --------------------------------------------------
    // UI Result
    // --------------------------------------------------

    if (success) {

      sendButton.textContent =
        "Sent ✓";

    } else {

      sendButton.textContent =
        "Send Message";
    }


    // --------------------------------------------------
    // Reset UI
    // --------------------------------------------------

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

    const previousState =
      !scrollToggle.checked;


    const command =
      scrollToggle.checked
        ? "SCROLL:ON"
        : "SCROLL:OFF";


    scrollToggle.disabled =
      true;


    const success =
      await sendCommand(
        command
      );


    if (!success) {

      scrollToggle.checked =
        previousState;
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

    const previousState =
      !displayToggle.checked;


    const command =
      displayToggle.checked
        ? "DISPLAY:ON"
        : "DISPLAY:OFF";


    displayToggle.disabled =
      true;


    const success =
      await sendCommand(
        command
      );


    if (!success) {

      displayToggle.checked =
        previousState;
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