"use strict";

const FC_URL = "https://fantasticcontraption.com";
const SELF_URL = "";

function self_url_full() {
	// https://stackoverflow.com/a/6257480
	// ends in /
	// no query parameters
	return location.protocol + '//' + location.host + location.pathname;
}

function chainElement(tags, content) {
    if (!tags || tags.length === 0) {
        if (typeof content === "string") {
            return document.createTextNode(content);
        }
        return content || document.createDocumentFragment();
    }
    let root = document.createElement(tags[0]);
    let current = root;
    for (let i = 1; i < tags.length; ++i) {
        let child = document.createElement(tags[i]);
        current.appendChild(child);
        current = child;
    }
    if (content !== undefined && content !== null) {
        if (typeof content === "string" || typeof content === "number") {
            current.appendChild(document.createTextNode(content));
        } else {
            current.appendChild(content);
        }
    }
    return root;
}

function linkElement(url, text) {
	const el = document.createElement("a");
    el.href = url;
    el.textContent = text === undefined ? url : text;
	return el;
}

let save_button  = document.getElementById("save");
let save_menu    = document.getElementById("save_menu");
let save_form    = document.getElementById("save_form");
let close_button = document.getElementById("close");
let design_link  = document.getElementById("link");
const accountButton = document.getElementById('account-button');
const accountMenu = document.getElementById('account_menu');
const closeAccountMenuButton = document.getElementById('close-account-menu');
const loginButton = document.getElementById('login-button');
const logoutButton = document.getElementById('logout-button');
const usernameField = document.getElementById('username-field');
const passwordField = document.getElementById('password-field');
const accountStatus = document.getElementById('account-status');
let version_button  = document.getElementById("version-button");
let version_menu  = document.getElementById("version_menu");
let version_commit_number  = document.getElementById("version_commit_number");
let version_branch_name  = document.getElementById("version_branch_name");
let version_is_dirty  = document.getElementById("version_is_dirty");
let version_sha  = document.getElementById("version_sha");
let close_version_menu  = document.getElementById("close-version-menu");

let user_id;

// Set the user ID within this session (does not affect persistent storage)
// Convention: falsy (including empty) value = logged out
function set_user_id(user_id_value) {
	user_id = user_id_value;
	if(user_id) {
		// Currently logged in
		accountStatus.textContent = "Logged in";
		save_button.disabled = false;
	} else {
		// Currently logged out
		accountStatus.textContent = "Logged out";
		save_button.disabled = true;
	}
}

set_user_id(localStorage.getItem("userId"));

let account_menu_opened = false;

function showAccountMenu() {
	accountMenu.style.display = 'block';
	account_menu_opened = true;
}

function hideAccountMenu() {
	accountMenu.style.display = 'none';
	account_menu_opened = false;
}

let version_menu_opened = false;

function showVersionMenu() {
	version_menu.style.display = 'block';
	version_menu_opened = true;
	// fetch version info lazily, provided by version.js
	let versionInfo = getVersionInfo();
	version_branch_name.textContent = versionInfo[0];
	version_is_dirty.textContent = versionInfo[1];
	version_commit_number.textContent = versionInfo[2];
	version_sha.textContent = versionInfo[3];
	console.log(versionInfo);
}

function hideVersionMenu() {
	version_menu.style.display = 'none';
	version_menu_opened = false;
}

function _loginOnTextReceived(text) {
	let parser = new DOMParser();
	let xml = parser.parseFromString(text, "text/xml");
	let user_id_tag = xml.getElementsByTagName("userId")[0];

	if (!user_id_tag) {
		accountStatus.textContent = "Login failed";
	} else {
		let user_id = user_id_tag.childNodes[0].nodeValue;
		localStorage.setItem("userId", user_id);
		set_user_id(user_id);
	}
}

function _loginOnRequestResult(response) {
	let text_promise = response.text();

	text_promise.then(_loginOnTextReceived);
}

function handleLogin(event) {
	event.preventDefault(); // Prevent default form submission
	const username = usernameField.value;
	const password = passwordField.value;

	accountStatus.textContent = "Attempting log in";

	let request_promise = fetch(FC_URL + "/logIn.php", {
		method: "POST",
		headers: {
			'Content-Type': 'application/x-www-form-urlencoded'
		},
		body: new URLSearchParams({
			"userName": username,
			"password": password,
		})
	});

	request_promise.then(_loginOnRequestResult);
}

function handleLogout() {
	localStorage.removeItem("userId");
	set_user_id(undefined);
	close(); // close save menu
}

let opened = false;

function play(event)
{
	inst.exports.key_down(65);
	play_button.blur();
}

function save(event)
{
	save_menu.style.display = "block";
	opened = true;
}

function close(event)
{
	save_menu.style.display = "none";
	design_link.style.display = "none";
	opened = false;
}

save_button.addEventListener("click", save);
close_button.addEventListener("click", close);
accountButton.addEventListener('click', showAccountMenu);
closeAccountMenuButton.addEventListener('click', hideAccountMenu);
loginButton.addEventListener('click', handleLogin);
logoutButton.addEventListener('click', handleLogout);
version_button.addEventListener('click', showVersionMenu);
close_version_menu.addEventListener('click', hideVersionMenu);

let canvas = document.getElementById("canvas");
let gl = canvas.getContext("webgl");
gl.getExtension("OES_element_index_uint");

let heap_counter = 0;
let heap = {};
let inst;

let param_string = window.location.search;
let params = new URLSearchParams(param_string);

function add_object(obj)
{
	let id = heap_counter++;
	heap[id] = obj;
	return id;
}

function get_object(id)
{
	return heap[id];
}

function make_view(data, size)
{
	return new DataView(inst.exports.memory.buffer, data, size);
}

function make_string(data, size)
{
	let decoder = new TextDecoder();
	return decoder.decode(make_view(data, size));
}

function make_cstring(data)
{
	let size = inst.exports.strlen(data);
	return make_string(data, size);
}

function debugRealClockSpeed(delayMs) {
	// Run a dummy function on a clock to see if the browser is ticking on an accurate clock
	const targetNumTicks = 50;
	const data = [undefined, -1, undefined]; // self handle, counter, first tick time
	data[0] = setInterval(function(){
		data[1] += 1;
		if(data[1] === 0) {
			data[2] = performance.now();
		} else if(data[1] >= targetNumTicks && data[0]) {
			clearInterval(data[0]);
			const realDelayMs = (performance.now() - data[2]) / data[1];
			const realTPS = 1000 / realDelayMs;
			console.log("Real clock: " + realDelayMs.toString() + " ms (" + realTPS.toString() + " Hz) (" + data[1].toString() + " ticks sampled)");
		}
	}, delayMs);
}

let gl_env = {
	glActiveTexture(texture) {
		gl.activeTexture(texture);
	},

	glAttachShader(program, shader) {
		gl.attachShader(get_object(program), get_object(shader));
	},

	glBindBuffer(target, buffer) {
		return gl.bindBuffer(target, get_object(buffer));
	},

	glBindTexture(target, texture) {
		return gl.bindTexture(target, get_object(texture));
	},

	glBufferData(target, size, data, usage) {
		gl.bufferData(target, make_view(data, size), usage);
	},

	glBufferSubData(target, offset, size, data) {
		gl.bufferSubData(target, offset, make_view(data, size));
	},

	glClear(mask) {
		gl.clear(mask);
	},

	glClearColor(red, green, blue, alpha) {
		gl.clearColor(red, green, blue, alpha);
	},

	glCompileShader(shader) {
		gl.compileShader(get_object(shader));
	},

	glCreateBuffer() {
		return add_object(gl.createBuffer());
	},

	glCreateProgram() {
		return add_object(gl.createProgram());
	},

	glCreateShader(type) {
		return add_object(gl.createShader(type));
	},

	glCreateTexture() {
		return add_object(gl.createTexture());
	},

	glDeleteShader(shader) {
		gl.deleteShader(get_object(shader));
	},

	glDisableVertexAttribArray(index) {
		gl.disableVertexAttribArray(index);
	},

	glDrawArrays(mode, first, count) {
		gl.drawArrays(mode, first, count);
	},

	glDrawElements(mode, count, type, offset) {
		gl.drawElements(mode, count, type, offset);
	},

	glEnableVertexAttribArray(index) {
		gl.enableVertexAttribArray(index);
	},

	glGetAttribLocation(program, name) {
		return gl.getAttribLocation(get_object(program), make_cstring(name));
	},

	glGetProgramParameter(program, pname) {
		return gl.getProgramParameter(get_object(program), pname);
	},

	glGetShaderParameter(shader, pname) {
		return gl.getShaderParameter(get_object(shader), pname);
	},

	glGetUniformLocation(program, name) {
		return add_object(gl.getUniformLocation(get_object(program), make_cstring(name)));
	},

	glLinkProgram(program) {
		gl.linkProgram(get_object(program));
	},

	glShaderSource(shader, count, string, length) {
		let strings = [];
		let lengths = [];
		let string_view = new DataView(inst.exports.memory.buffer, string);

		if (length == 0) {
			for (let i = 0; i < count; i++) {
				let str = string_view.getUint32(i * 4, true);
				let len = inst.exports.strlen(str);
				lengths.push(len);
			}
		} else {
			/* TODO */
		}

		for (let i = 0; i < count; i++) {
			let str = string_view.getUint32(i * 4, true);
			strings.push(make_string(str, lengths[i]));
		}

		let source = strings.join("");
		gl.shaderSource(get_object(shader), source);
	},

	glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels) {
		/* TODO: how many bytes per pixel? */
		let pixel_data = new Uint8Array(inst.exports.memory.buffer, pixels, width * height);
		gl.texImage2D(target, level, internalformat, width, height, border, format, type, pixel_data);
	},

	glTexParameteri(target, pname, param) {
		gl.texParameteri(target, pname, param);
	},

	glUniform1i(location, v0) {
		gl.uniform1i(get_object(location), v0);
	},

	glUniform2f(location, v0, v1) {
		gl.uniform2f(get_object(location), v0, v1);
	},

	glUniform3f(location, v0, v1, v2) {
		gl.uniform3f(get_object(location), v0, v1, v2);
	},

	glUseProgram(program) {
		gl.useProgram(get_object(program));
	},

	glVertexAttribPointer(index, size, type, normalized, stride, pointer) {
		gl.vertexAttribPointer(index, size, type, normalized, stride, pointer);
	},

	glViewport(x, y, width, height) {
	},

	set_interval(func, delay, arg) {
		console.log("Set clock to " + delay.toString() + " ms (expected " + (1000/delay).toString() + " Hz)");
		debugRealClockSpeed(delay);
		return setInterval(inst.exports.call, delay, func, arg);
	},

	time_precise_ms() {
		return performance.now();
	},

	clear_interval(id) {
		clearInterval(id);
	},

	print_slice(str, len) {
		console.log(make_string(str, len));
	},

	printf(fmt, args) {
		let fmt_view = new DataView(inst.exports.memory.buffer, fmt);
		let arg_view = new DataView(inst.exports.memory.buffer, args);
		let res = [];
		let i = 0;
		let a = 0;

		while (true) {
			let c = fmt_view.getUint8(i);
			if (c == 37) { // %
				i++;
				let cc = fmt_view.getUint8(i);
				if (cc == 100) { // d
					let d = arg_view.getInt32(a, true);
					res.push(d.toString());
					a += 4;
				} else if (cc == 102) { // f
					a += (-a) & 7;
					let f = arg_view.getFloat64(a, true);
					res.push(f.toString());
					a += 8;
				} else if (cc == 117) { // u
					let u = arg_view.getUint32(a, true);
					res.push(u.toString());
					a += 4;
				}
			} else if (!c) {
				break;
			} else {
				res.push(String.fromCharCode(c));
			}
			i++;
		}

		console.log(res.join(""));
	}
};

let import_object = {
	env: gl_env,
};

function canvas_draw(timestamp)
{
	var width  = canvas.clientWidth;
	var height = canvas.clientHeight;
	if (canvas.width != width || canvas.height != height) {
		canvas.width = width;
		canvas.height = height;
		gl.viewport(0, 0, width, height);
		inst.exports.resize(width, height);
	}
	inst.exports.draw();
	window.requestAnimationFrame(canvas_draw);
}

function to_key(code)
{
	if (code == "Space") return 65;
	if (code == "KeyR") return 27;
	if (code == "KeyM") return 58;
	if (code == "KeyS") return 39;
	if (code == "KeyD") return 40;
	if (code == "KeyU") return 30;
	if (code == "KeyW") return 25;
	if (code == "KeyC") return 54;
	if (code == "Digit1") return 10;
	if (code == "Digit2") return 11;
	if (code == "Digit3") return 12;
	if (code == "Digit4") return 13;
	if (code == "Digit5") return 14;
	if (code == "Digit6") return 15;
	if (code == "Digit7") return 16;
	if (code == "Digit8") return 17;
	if (code == "Digit9") return 18;
	if (code == "Digit0") return 19;
	if (code == "ShiftLeft") return 50;
	if (code == "ControlLeft") return 37;
	return 0;
}

function to_button(code)
{
	if (code == 0) return 1;
	return 0;
}

function canvas_keydown(event)
{
	if (opened || account_menu_opened || version_menu_opened)
		return;
	inst.exports.key_down(to_key(event.code));
}

function canvas_keyup(event)
{
	if (opened || account_menu_opened || version_menu_opened)
		return;
	inst.exports.key_up(to_key(event.code));
}

function canvas_mousedown(event)
{
	inst.exports.button_down(to_button(event.button));
}

function canvas_mouseup(event)
{
	inst.exports.button_up(to_button(event.button));
}

function canvas_mousemove(event)
{
	inst.exports.move(event.offsetX, event.offsetY);
}

function canvas_wheel(event)
{
	inst.exports.scroll(-0.02 * event.deltaY);
}

function alloc_str(str)
{
	let encoder = new TextEncoder();

	let str_uint8 = encoder.encode(str);
	let len = str_uint8.length;
	let mem = inst.exports.malloc(len + 1);
	let mem_uint8 = new Uint8Array(inst.exports.memory.buffer, mem, len + 1);
	mem_uint8.set(str_uint8);
	mem_uint8[len] = 0;

	return mem;
}

function on_text(text)
{
	console.log(text);
	design_link.innerHTML = design_link.href = self_url_full() + "?designId=" + text;
	design_link.style.display = "block";
}

function on_result(response)
{
	let text_promise = response.text();

	text_promise.then(on_text);
}

function save_design(event)
{
	event.preventDefault();

	let data = new FormData(save_form);

	let user = alloc_str(user_id);
	let name = alloc_str(data.get("name"));
	let desc = alloc_str(data.get("description"));

	let xml = inst.exports.export(user, name, desc);
	let len = inst.exports.strlen(xml);

	let xml_str = make_cstring(xml);

	let request_promise = fetch(FC_URL + "/saveDesign.php", {
		method: "POST",
		headers: {
			'Content-Type': 'application/x-www-form-urlencoded'
		},
		body: new URLSearchParams({ "xml": xml_str })
	});

	request_promise.then(on_result);
}

function init_module(results)
{
	let module = results[0];
	let buffer = results[1];

	inst = module.instance;

	let buffer_uint8 = new Uint8Array(buffer);
	let len = buffer_uint8.length;
	let mem = inst.exports.malloc(len);
	let mem_uint8 = new Uint8Array(inst.exports.memory.buffer, mem, len);
	mem_uint8.set(buffer_uint8);

	inst.exports.init(mem, buffer_uint8.length);
	inst.exports.resize(canvas.width, canvas.height);
	window.requestAnimationFrame(canvas_draw);
	addEventListener("keydown", canvas_keydown);
	addEventListener("keyup", canvas_keyup);
	canvas.addEventListener("mousedown", canvas_mousedown);
	canvas.addEventListener("mouseup", canvas_mouseup);
	canvas.addEventListener("mousemove", canvas_mousemove);
	canvas.addEventListener("wheel", canvas_wheel);
	save_form.addEventListener("submit", save_design);
}

let module_promise = WebAssembly.instantiateStreaming(
	fetch("fcsim.wasm"), import_object
);

let design_id = params.get('designId');
let level_id = params.get('levelId');

// homepage behaviour: default brown
if(!design_id && !level_id) {
    design_id = '12706185';

    // show usage
    const notification = document.getElementById("notification");
    if (notification) {
        const helpDiv = document.createElement("div");

        helpDiv.appendChild(chainElement(["p"], "To load levels or designs, use ?levelId or ?designId"));
		helpDiv.appendChild(chainElement(["p", "b"], "Examples"));
		helpDiv.appendChild(chainElement(["p"], linkElement(self_url_full() + "?levelId=646726")));
		helpDiv.appendChild(chainElement(["p"], linkElement(self_url_full() + "?designId=12483401")));

        notification.appendChild(helpDiv);
    }
}

let response_promise = fetch(FC_URL + "/retrieveLevel.php", {
	method: "POST",
	headers: {
		'Content-Type': 'application/x-www-form-urlencoded'
	},
	body: new URLSearchParams({
		"id":         design_id ? design_id : level_id,
		"loadDesign": design_id ? "1" : "0",
	})
});

let buffer_promise = response_promise.then((resp) => resp.arrayBuffer());

let module_buffer_promise = Promise.all([module_promise, buffer_promise]);

module_buffer_promise.then(init_module);
