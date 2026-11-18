// EXPERIMENT - DO NOT MERGE. Why does saveDesign.php return 500 for designs
// opened from a generated file (tools/fsin_threshold_designs.py)?
// Opt-in: after opening a file, add &probeSave=1 to the URL and reload, while
// logged in. Saves the loaded design several times, each variant changing one
// thing, and reports the server's reply for each. Every successful variant
// creates a real design on the logged-in account.

const PROBE_SAVE_VARIANTS = [
  // label (also the design name, max 15 chars), transform of the exported XML
  ["p asis", (x) => x],
  [
    "p level",
    (x) => x.replace(/<levelId>[^<]*<\/levelId>/, "<levelId>646726</levelId>"),
  ],
  [
    "p small angle",
    (x) =>
      x.replace(/<rotation>([^<]*)<\/rotation>/g, (m, v) => {
        const r = parseFloat(v);
        return Math.abs(r) > 2 * Math.PI
          ? `<rotation>${r % (2 * Math.PI)}</rotation>`
          : m;
      }),
  ],
  [
    "p one goal",
    (x) => {
      const a = x.indexOf("<playerBlocks>") + "<playerBlocks>".length;
      const b = x.indexOf("</playerBlocks>");
      const first = x.slice(a, b).match(/^<(\w+)[^>]*>.*?<\/\1>/s);
      return x.slice(0, a) + (first ? first[0] : "") + x.slice(b);
    },
  ],
  ["p no circles", (x) => x.replace(/<StaticCircle>.*?<\/StaticCircle>/gs, "")],
  [
    "p no zero size",
    (x) =>
      x.replace(
        /<(width|height)>(-?0(\.0*)?)<\/\1>/g,
        (m, t) => `<${t}>1</${t}>`,
      ),
  ],
  [
    "p tiny",
    (x) =>
      x.replace(
        /<level>.*<\/level>/s,
        "<level><levelBlocks><StaticRectangle><rotation>0</rotation><position><x>0</x><y>300</y></position><width>800</width><height>40</height><goalBlock>false</goalBlock><joints></joints></StaticRectangle></levelBlocks>" +
          '<playerBlocks><NoSpinWheel id="1"><rotation>0</rotation><position><x>0</x><y>0</y></position><width>40</width><height>40</height><goalBlock>true</goalBlock><joints></joints></NoSpinWheel></playerBlocks>' +
          "<start><position><x>0</x><y>0</y></position><width>400</width><height>400</height></start>" +
          "<end><position><x>0</x><y>200</y></position><width>400</width><height>200</height></end></level>",
      ),
  ],
];

function probe_save_show(text) {
  let pre = document.getElementById("probe-save-out");
  if (!pre) {
    pre = document.createElement("pre");
    pre.id = "probe-save-out";
    pre.style.cssText =
      "position:fixed;top:0;right:0;width:45%;max-height:90%;overflow:auto;" +
      "background:#fff;color:#000;border:2px solid red;padding:8px;" +
      "z-index:9999;font-size:11px;white-space:pre-wrap;";
    document.body.appendChild(pre);
  }
  pre.textContent = text;
}

async function probe_save_post(xml) {
  const out = {};
  try {
    const r = await fetch(FC_URL + "/saveDesign.php", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({ xml: xml }),
    });
    out.status = r.status;
    out.body = (await r.text()).slice(0, 300);
  } catch (err) {
    out.error = String(err);
  }
  return out;
}

async function probe_save() {
  if (!user_id) {
    probe_save_show("probe: log in first, then reload");
    return;
  }
  if (
    !confirm(
      `Save ${PROBE_SAVE_VARIANTS.length} probe designs to this account?`,
    )
  )
    return;
  const exported = make_cstring(
    inst.exports.do_export(
      alloc_str(user_id),
      alloc_str("PROBE"),
      alloc_str("save probe, safe to delete"),
    ),
  );
  const report = {
    page: location.href,
    time: new Date().toISOString(),
    xmlLength: exported.length,
    results: [],
  };
  for (const [label, transform] of PROBE_SAVE_VARIANTS) {
    const xml = transform(exported).replace(
      "<name>PROBE</name>",
      `<name>${label}</name>`,
    );
    probe_save_show(`probe: sending "${label}"...`);
    report.results.push({
      label,
      xmlLength: xml.length,
      ...(await probe_save_post(xml)),
    });
  }
  const text = JSON.stringify(report, null, 2);
  console.log(text);
  probe_save_show(text);
}

if (new URLSearchParams(location.search).get("probeSave")) {
  const wait = setInterval(() => {
    if (typeof inst !== "undefined" && inst) {
      clearInterval(wait);
      probe_save();
    }
  }, 200);
}
