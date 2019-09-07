

let renderer = new MicroRenderer(document.querySelector('#glcanvas'));

let i = 0;
// Draw the scene repeatedly
function render() {
    i++;
    renderer.draw();
    requestAnimationFrame(render);
}


// Let us open a web socket
let ws = new WebSocket("ws://192.168.1.150:8000/hello");
ws.binaryType = "arraybuffer";

ws.onopen = function (event) {
    ws.send("test");
};

ws.onmessage = function (evt) {
    let received_msg = evt.data;
    renderer.updateTexture(1920, 1080, new Uint8Array(evt.data));
    ws.send("test");
};


requestAnimationFrame(render);