const SERVICE_UUID =
  "c7a10001-6c9e-4d5d-a001-123456789abc";

const CHARACTERISTIC_UUID =
  "c7a10002-6c9e-4d5d-a001-123456789abc";


let device = null;
let characteristic = null;


// ==================== Elements ====================

const connectButton =
  document.getElementById("connectButton");

const sendButton =
  document.getElementById("sendButton");

const messageInput =
  document.getElementById("messageInput");

const scrollToggle =
  document.getElementById("scrollToggle");

const displayToggle =
  document.getElementById("displayToggle");

const statusText =
  document.getElementById("statusText");

const statusBadge =
  document.getElementById("statusBadge");

const characterCount =
  document.getElementById("characterCount");

const log =
  document.getElementById("log");


// ==================== UI State ====================

function setControlsEnabled(enabled) {

  messageInput.disabled = !enabled;

  sendButton.disabled = !enabled;

  scrollToggle.disabled = !enabled;

  displayToggle.disabled = !enabled;
}


function setConnectionStatus(connected) {

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

    setControlsEnabled(true);

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

    setControlsEnabled(false);
  }
}


// ==================== Connect ====================

async function connectHairClip() {

  try {

    log.textContent =
      "Searching for HairClip...";


    device =
      await navigator.bluetooth.requestDevice({

        filters: [
          {
            name: "HairClip-V1"
          }
        ],

        optionalServices: [
          SERVICE_UUID
        ]

      });


    device.addEventListener(
      "gattserverdisconnected",
      handleDisconnected
    );


    log.textContent =
      "Connecting...";


    const server =
      await device.gatt.connect();


    const service =
      await server.getPrimaryService(
        SERVICE_UUID
      );


    characteristic =
      await service.getCharacteristic(
        CHARACTERISTIC_UUID
      );


    setConnectionStatus(true);

    log.textContent =
      `Connected to ${device.name}`;

  } catch (error) {

    console.error(error);

    setConnectionStatus(false);

    log.textContent =
      `Connection failed: ${error.message}`;
  }
}


// ==================== Disconnect ====================

function disconnectHairClip() {

  if (
    device &&
    device.gatt.connected
  ) {

    device.gatt.disconnect();
  }
}


function handleDisconnected() {

  characteristic = null;

  setConnectionStatus(false);

  log.textContent =
    "HairClip disconnected";
}


// ==================== BLE Command ====================

async function sendCommand(command) {

  if (!characteristic) {

    log.textContent =
      "Connect HairClip first";

    return false;
  }


  try {

    const encoder =
      new TextEncoder();


    await characteristic.writeValue(
      encoder.encode(command)
    );


    log.textContent =
      `Sent: ${command}`;

    return true;

  } catch (error) {

    console.error(error);

    log.textContent =
      `Send failed: ${error.message}`;

    return false;
  }
}


// ==================== Connect Button ====================

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


// ==================== Message ====================

messageInput.addEventListener(
  "input",
  () => {

    characterCount.textContent =
      `${messageInput.value.length} / 100`;
  }
);


sendButton.addEventListener(
  "click",
  async () => {

    const text =
      messageInput.value.trim();


    if (!text) {

      log.textContent =
        "Type a message first";

      return;
    }


    sendButton.disabled = true;

    sendButton.textContent =
      "Sending...";


    const success =
      await sendCommand(
        `TEXT:${text}`
      );


    sendButton.textContent =
      success
        ? "Sent ✓"
        : "Send Message";


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


messageInput.addEventListener(
  "keydown",
  event => {

    if (event.key === "Enter") {

      event.preventDefault();

      sendButton.click();
    }
  }
);


// ==================== Scroll ====================

scrollToggle.addEventListener(
  "change",
  async () => {

    const command =
      scrollToggle.checked
        ? "SCROLL:ON"
        : "SCROLL:OFF";


    const success =
      await sendCommand(command);


    if (!success) {

      // คืน toggle กลับถ้าส่งไม่สำเร็จ
      scrollToggle.checked =
        !scrollToggle.checked;
    }
  }
);


// ==================== Display ====================

displayToggle.addEventListener(
  "change",
  async () => {

    const command =
      displayToggle.checked
        ? "DISPLAY:ON"
        : "DISPLAY:OFF";


    const success =
      await sendCommand(command);


    if (!success) {

      displayToggle.checked =
        !displayToggle.checked;
    }
  }
);


// ==================== Initial UI ====================

setConnectionStatus(false);