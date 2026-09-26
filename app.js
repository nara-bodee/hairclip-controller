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
// CONNECTION STATUS
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


    connectButton.textContent =
      "Connect HairClip";


    setControlsEnabled(
      false
    );
  }
}


// ======================================================
// SYNC STATE FROM HAIRCLIP
// ======================================================

async function syncControllerState() {

  if (!characteristic) {
    return;
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


    // --------------------------------------------------
    // รูปแบบข้อมูล:
    //
    // 1,1
    // HELLO WORLD
    //
    // บรรทัด 1 = Scroll, Display
    // บรรทัด 2 = Message
    // --------------------------------------------------


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
    // SCROLL / DISPLAY STATE
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


    const scrollState =
      parts[0];


    const displayState =
      parts[1];


    scrollToggle.checked =
      scrollState === "1";


    displayToggle.checked =
      displayState === "1";


    // ==================================================
    // MESSAGE
    // ==================================================

    messageInput.value =
      currentMessage;


    characterCount.textContent =
      `${currentMessage.length} / 100`;


    log.textContent =
      "HairClip state synchronized";


  } catch (error) {

    console.error(
      error
    );


    log.textContent =
      "Connected, but state sync failed";
  }
}


// ======================================================
// CONNECT
// ======================================================

async function connectHairClip() {

  try {

    log.textContent =
      "Searching for HairClip...";


    // --------------------------------------------------
    // เปิด Bluetooth Device Picker
    // --------------------------------------------------

    device =
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


    // --------------------------------------------------
    // Disconnect Event
    // --------------------------------------------------

    device.addEventListener(

      "gattserverdisconnected",

      handleDisconnected

    );


    log.textContent =
      "Connecting...";


    // --------------------------------------------------
    // Connect GATT
    // --------------------------------------------------

    const server =
      await device.gatt.connect();


    // --------------------------------------------------
    // Get Service
    // --------------------------------------------------

    const service =
      await server.getPrimaryService(
        SERVICE_UUID
      );


    // --------------------------------------------------
    // Get Characteristic
    // --------------------------------------------------

    characteristic =
      await service.getCharacteristic(
        CHARACTERISTIC_UUID
      );


    // --------------------------------------------------
    // UI Connected
    // --------------------------------------------------

    setConnectionStatus(
      true
    );


    // --------------------------------------------------
    // โหลด State จริงจาก ESP32
    // --------------------------------------------------

    await syncControllerState();


    log.textContent =
      `Connected to ${device.name}`;


  } catch (error) {

    console.error(
      error
    );


    setConnectionStatus(
      false
    );


    log.textContent =
      `Connection failed: ${error.message}`;
  }
}


// ======================================================
// DISCONNECT
// ======================================================

function disconnectHairClip() {

  if (
    device &&
    device.gatt.connected
  ) {

    device.gatt.disconnect();
  }
}


// ======================================================
// HANDLE DISCONNECT
// ======================================================

function handleDisconnected() {

  characteristic =
    null;


  setConnectionStatus(
    false
  );


  log.textContent =
    "HairClip disconnected";
}


// ======================================================
// SEND BLE COMMAND
// ======================================================

async function sendCommand(
  command
) {

  if (!characteristic) {

    log.textContent =
      "Connect HairClip first";

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

    console.error(
      error
    );


    log.textContent =
      `Send failed: ${error.message}`;


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
    // Sending UI
    // --------------------------------------------------

    sendButton.disabled =
      true;


    sendButton.textContent =
      "Sending...";


    // --------------------------------------------------
    // Send
    // --------------------------------------------------

    const success =
      await sendCommand(
        `TEXT:${text}`
      );


    // --------------------------------------------------
    // Result UI
    // --------------------------------------------------

    if (success) {

      sendButton.textContent =
        "Sent ✓";

    } else {

      sendButton.textContent =
        "Send Message";
    }


    // --------------------------------------------------
    // Reset Button Text
    // --------------------------------------------------

    setTimeout(

      () => {

        sendButton.textContent =
          "Send Message";


        if (
          device &&
          device.gatt.connected
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

      sendButton.click();
    }
  }

);


// ======================================================
// SCROLL TOGGLE
// ======================================================

scrollToggle.addEventListener(

  "change",

  async () => {

    const command =
      scrollToggle.checked

        ? "SCROLL:ON"

        : "SCROLL:OFF";


    const success =
      await sendCommand(
        command
      );


    // --------------------------------------------------
    // ถ้าส่งไม่สำเร็จ
    // คืน Toggle กลับ
    // --------------------------------------------------

    if (!success) {

      scrollToggle.checked =
        !scrollToggle.checked;
    }
  }

);


// ======================================================
// DISPLAY TOGGLE
// ======================================================

displayToggle.addEventListener(

  "change",

  async () => {

    const command =
      displayToggle.checked

        ? "DISPLAY:ON"

        : "DISPLAY:OFF";


    const success =
      await sendCommand(
        command
      );


    // --------------------------------------------------
    // ถ้าส่งไม่สำเร็จ
    // คืน Toggle กลับ
    // --------------------------------------------------

    if (!success) {

      displayToggle.checked =
        !displayToggle.checked;
    }
  }

);


// ======================================================
// INITIAL UI
// ======================================================

setConnectionStatus(
  false
);