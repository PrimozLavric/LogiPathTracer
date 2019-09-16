// Nodejs-like Buffer built-in
let renderer = new MicroRenderer(document.querySelector('#glcanvas'));

var LZ4 = require('lz4');

let i = 0;

// Draw the scene repeatedly

function render() {
    i++;
    renderer.draw();
    requestAnimationFrame(render);
}

// Let us open a web socket
let ws = new WebSocket("ws://" + location.hostname + ":10000");
ws.binaryType = "arraybuffer";

let resX = 1920;
let resY = 1200;

let imageSize = resX * resY * 3;
let prevData = new Uint8Array(resX * resY * 3);
let diff = true;
let lz = true;

ws.onopen = function (event) {
    ws.send([0]);
};


let byteData = new Uint8Array(imageSize + Math.ceil(imageSize / 8.0));
ws.onmessage = function (evt) {
    if (lz) {
        LZ4.decodeBlock(new Uint8Array(evt.data), byteData);
    } else {
        byteData = new Uint8Array(evt.data);
    }

    if (diff) {
        let currentBit = 0;
        let currentSignByte = imageSize;
        for (let i = 0; i < imageSize; i++) {
            if ((byteData[currentSignByte] & (1 << currentBit)) != 0) {
                prevData[i] -= byteData[i];
            } else {
                prevData[i] += byteData[i];
            }

            if (++currentBit >= 8) {
                currentBit = 0;
                currentSignByte++;
            }
        }


        renderer.updateTexture(resX, resY, prevData);
    } else {
        renderer.updateTexture(resX, resY, byteData);
    }

    ws.send([0]);
};

requestAnimationFrame(render);