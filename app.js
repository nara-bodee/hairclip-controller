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

const statusDot =
  document.getElementById("statusDot");

const log =
  document.getElementById("log");


// ==================== Status ====================

function setStatus(connected) {

  if (connected) {

    statusText.textContent =
      "Connected";

    statusDot.classList.remove(
      "disconnected"
    );

    statusDot.classList.add(
      "connected"
    );

    connectButton.textContent =
      "Disconnect";

  } else {

    statusText.textContent =
      "Disconnected";

    statusDot.classList.remove(
      "connected"
    );

    statusDot.classList.add(
      "disconnected"
    );

    connectButton.textContent =
      "Connect";
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


    setStatus(true);

    log.textContent =
      "HairClip connected";

  } catch (error) {

    console.error(error);

    log.textContent =
      "Connection failed";
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

  setStatus(false);

  log.textContent =
    "HairClip disconnected";
}


// ==================== Send BLE Command ====================

async function sendCommand(command) {

  if (!characteristic) {

    log.textContent =
      "Connect HairClip first";

    return;
  }


  try {

    const encoder =
      new TextEncoder();


    await characteristic.writeValue(
      encoder.encode(command)
    );


    log.textContent =
      `Sent: ${command}`;

  } catch (error) {

    console.error(error);

    log.textContent =
      "Send failed";
  }
}


// ==================== Buttons ====================

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


sendButton.addEventListener(
  "click",
  () => {

    const text =
      messageInput.value.trim();


    if (!text) {
      return;
    }


    sendCommand(
      `TEXT:${text}`
    );
  }
);


// Enter = Send

messageInput.addEventListener(
  "keydown",
  event => {

    if (event.key === "Enter") {

      sendButton.click();
    }
  }
);


// ==================== Scroll ====================

scrollToggle.addEventListener(
  "change",
  () => {

    if (scrollToggle.checked) {

      sendCommand(
        "SCROLL:ON"
      );

    } else {

      sendCommand(
        "SCROLL:OFF"
      );
    }
  }
);


// ==================== Display ====================

displayToggle.addEventListener(
  "change",
  () => {

    if (displayToggle.checked) {

      sendCommand(
        "DISPLAY:ON"
      );

    } else {

      sendCommand(
        "DISPLAY:OFF"
      );
    }
  }
);