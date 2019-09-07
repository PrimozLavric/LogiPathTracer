
const vsSource = `
    attribute vec3 aVertexPosition;
    attribute vec2 aTextureCoord;

    varying highp vec2 vTextureCoord;

    void main(void) {
      gl_Position = vec4(aVertexPosition, 1.0);
      vTextureCoord = aTextureCoord;
    }
  `;

// Fragment shader program

const fsSource = `
    varying highp vec2 vTextureCoord;

    uniform sampler2D uSampler;

    void main(void) {
      gl_FragColor = vec4(texture2D(uSampler, vTextureCoord).xyz, 1.0);
    }
  `;

class MicroRenderer {
    constructor(canvas) {
        this.canvas = canvas;
        this.gl = this.canvas.getContext('webgl');

        if (!this.gl) {
            console.error('Unable to initialize WebGL. Your browser or machine may not support it.');
            return;
        }

        // Initialize a shader program
        const shaderProgram = this._initShaderProgram(vsSource, fsSource);

        // Collect the shader program info
        this.programInfo = {
            program: shaderProgram,
            attribLocations: {
                vertexPosition: this.gl.getAttribLocation(shaderProgram, 'aVertexPosition'),
                textureCoord: this.gl.getAttribLocation(shaderProgram, 'aTextureCoord'),
            },
            uniformLocations: {
                uSampler: this.gl.getUniformLocation(shaderProgram, 'uSampler'),
            },
        };

        this.buffers = this._initBuffers();
        this.texture = this.gl.createTexture();
        this.updateTexture(1, 1, new Uint8Array([60, 60, 60]));

        this.gl.clearColor(0.0, 0.0, 0.0, 1.0);
        this.gl.clearDepth(1.0);
        this.gl.enable(this.gl.DEPTH_TEST);
        this.gl.depthFunc(this.gl.LEQUAL);
    }

    _initShaderProgram(vsSource, fsSource) {
        const vertexShader = this._loadShader(this.gl.VERTEX_SHADER, vsSource);
        const fragmentShader = this._loadShader(this.gl.FRAGMENT_SHADER, fsSource);

        // Create the shader program
        const shaderProgram = this.gl.createProgram();
        this.gl.attachShader(shaderProgram, vertexShader);
        this.gl.attachShader(shaderProgram, fragmentShader);
        this.gl.linkProgram(shaderProgram);

        // If creating the shader program failed, alert
        if (!this.gl.getProgramParameter(shaderProgram, this.gl.LINK_STATUS)) {
            console.error('Unable to initialize the shader program: ' + this.gl.getProgramInfoLog(shaderProgram));
            return null;
        }

        return shaderProgram;
    }

    _loadShader(type, source) {
        const shader = this.gl.createShader(type);

        // Send the source to the shader object
        this.gl.shaderSource(shader, source);

        // Compile the shader program
        this.gl.compileShader(shader);

        // See if it compiled successfully
        if (!this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS)) {
            console.error('An error occurred compiling the shaders: ' + this.gl.getShaderInfoLog(shader));
            this.gl.deleteShader(shader);
            return null;
        }

        return shader;
    }

    _initBuffers() {
        // Positions buffer.
        const positionBuffer = this.gl.createBuffer();

        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, positionBuffer);
        const positions = [
            // Front face
            -1.0, -1.0, 0.0,
            1.0, -1.0, 0.0,
            1.0, 1.0, 0.0,
            -1.0, 1.0, 0.0
        ];

        this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array(positions), this.gl.STATIC_DRAW);

        // Texture coordinate buffer.
        const textureCoordBuffer = this.gl.createBuffer();
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, textureCoordBuffer);

        const textureCoordinates = [
            // Front
            1.0, 1.0,
            0.0, 1.0,
            0.0, 0.0,
            1.0, 0.0,
        ];

        this.gl.bufferData(this.gl.ARRAY_BUFFER, new Float32Array(textureCoordinates),
            this.gl.STATIC_DRAW);

        // Index buffer
        const indexBuffer = this.gl.createBuffer();
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, indexBuffer);

        const indices = [
            0, 1, 2, 0, 2, 3    // front
        ];

        this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER,
            new Uint16Array(indices), this.gl.STATIC_DRAW);

        return {
            position: positionBuffer,
            textureCoord: textureCoordBuffer,
            indices: indexBuffer,
        };
    }

    updateTexture(width, height, data) {
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.LINEAR);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.LINEAR);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);

        const level = 0;
        const internalFormat = this.gl.RGB;
        const border = 0;
        const srcFormat = this.gl.RGB;
        const srcType = this.gl.UNSIGNED_BYTE;
        this.gl.texImage2D(this.gl.TEXTURE_2D, level, internalFormat,
            width, height, border, srcFormat, srcType,
            data);
    }

    draw() {
        // Clear the canvas before we start drawing on it.
        this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);

        // Tell WebGL how to pull out the positions from the position
        // buffer into the vertexPosition attribute
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffers.position);
        this.gl.vertexAttribPointer(this.programInfo.attribLocations.vertexPosition,3, this.gl.FLOAT,false,0,0);
        this.gl.enableVertexAttribArray(this.programInfo.attribLocations.vertexPosition);


        // Tell WebGL how to pull out the texture coordinates from
        // the texture coordinate buffer into the textureCoord attribute.
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.buffers.textureCoord);
        this.gl.vertexAttribPointer(
            this.programInfo.attribLocations.textureCoord, 2, this.gl.FLOAT, false, 0, 0);
        this.gl.enableVertexAttribArray(
            this.programInfo.attribLocations.textureCoord);

        // Tell WebGL which indices to use to index the vertices
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.buffers.indices);

        // Tell WebGL to use our program when drawing

        this.gl.useProgram(this.programInfo.program);

        // Specify the texture to map onto the faces.

        // Tell WebGL we want to affect texture unit 0
        this.gl.activeTexture(this.gl.TEXTURE0);

        // Bind the texture to texture unit 0
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);

        // Tell the shader we bound the texture to texture unit 0
        this.gl.uniform1i(this.programInfo.uniformLocations.uSampler, 0);

        this.gl.drawElements(this.gl.TRIANGLES, 6, this.gl.UNSIGNED_SHORT, 0);
    }
}