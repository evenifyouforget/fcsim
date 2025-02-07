function on_text(text) {
	let parser = new DOMParser();
	let xml = parser.parseFromString(text, "text/xml");
	let user_id_tag = xml.getElementsByTagName("userId")[0];

	if (!user_id_tag) {
		login_result.innerHTML = "Login failed";
	} else {
		login_result.innerHTML = "Login successful";
		let user_id = user_id_tag.childNodes[0].nodeValue;
		localStorage.setItem("userId", user_id);
	}
}

function on_result(response) {
	let text_promise = response.text();

	text_promise.then(on_text);
}

function on_submit(event) {
	event.preventDefault();

	let data = new FormData(login);

	let request_promise = fetch("http://fantasticcontraption.com/logIn.php", {
		method: "POST",
		headers: {
			'Content-Type': 'application/x-www-form-urlencoded'
		},
		body: new URLSearchParams({
			"userName": data.get("username"),
			"password": data.get("password"),
		})
	});

	request_promise.then(on_result);
}

login.onsubmit = on_submit;