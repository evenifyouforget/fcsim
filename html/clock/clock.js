window.onload = function() {

const input = document.getElementById('numberInput');
const button = document.getElementById('updateButton');
const output = document.getElementById('numberOutput');
const output2 = document.getElementById('numberOutput2');

const MAX_SAMPLES = 512;
const MAX_WINDOW_MS = 3000;

let handle = undefined;
let samples = [];

function tickFunc() {
  let precise_time_ms = performance.now();
  samples.push(precise_time_ms);
  while(samples.length > MAX_SAMPLES) {
  	samples.shift();
  }
  if(samples.length >= 2) {
  	// try to calculate tps
    let window_last = samples[samples.length - 1];
    let i = samples.length - 2;
    while(i-1 >= 0 && window_last - samples[i-1] <= MAX_WINDOW_MS) {
    	i--;
    }
    let window_first = samples[i];
    let tps = 1000 * (samples.length - 1 - i) / (window_last - window_first);
  	output2.textContent = tps.toString();
  }
}

function resetClock(tickMs) {
	samples = [];
  output.textContent = (1000 / tickMs).toString();
  output2.textContent = "...";
	if(handle) {
  	clearInterval(handle);
  }
  handle = setInterval(tickFunc, tickMs);
}

button.addEventListener('click', () => {
  const value = Number(input.value || 33);
  resetClock(value);
});

resetClock(33);

}