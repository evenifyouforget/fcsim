// EXPERIMENT FOR ISSUE #29 - DO NOT MERGE.
// Opt-in with ?probe29=1. Saves the loaded design several times with the
// <levelId> swapped, to see whether saveDesign.php rejects purged level IDs.
// Each probe creates a real design on the logged-in account.

const PROBE29_VARIANTS = [
  ["asis", undefined], // exactly what the Save button sends
  ["blank", ""], // no level link
  ["wall", "31"], // known-good level (The Wall)
  ["edge", "3024"], // purged level (On the Edge)
];
const PROBE29_DESIGNS = ["12668310", "12635400"];

function probe29_show(text) {
  let pre = document.getElementById("probe29-out");
  if (!pre) {
    pre = document.createElement("pre");
    pre.id = "probe29-out";
    pre.style.cssText =
      "position:fixed;top:0;right:0;width:45%;max-height:90%;overflow:auto;" +
      "background:#fff;color:#000;border:2px solid red;padding:8px;" +
      "z-index:9999;font-size:11px;white-space:pre-wrap;";
    document.body.appendChild(pre);
  }
  pre.textContent = text;
  return pre;
}

async function probe29_post(xml) {
  const out = {};
  try {
    const r = await fetch(FC_URL + "/saveDesign.php", {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({ xml: xml }),
    });
    out.status = r.status;
    out.contentType = r.headers.get("content-type");
    out.body = (await r.text()).slice(0, 500);
  } catch (err) {
    out.error = String(err);
  }
  return out;
}

async function probe29_run() {
  const report = {
    page: location.href,
    designId: params.get("designId"),
    time: new Date().toISOString(),
    userAgent: navigator.userAgent,
    results: [],
  };
  const exported = make_cstring(
    inst.exports.do_export(
      alloc_str(user_id),
      alloc_str("PROBE"),
      alloc_str("issue 29 probe, safe to delete"),
    ),
  );
  const m = exported.match(/<levelId>([^<]*)<\/levelId>/);
  report.exportedLevelId = m ? m[1] : null;
  report.xmlLength = exported.length;

  for (const [label, levelId] of PROBE29_VARIANTS) {
    let xml = exported.replace(
      "<name>PROBE</name>",
      `<name>i29 probe ${report.designId} ${label}</name>`,
    );
    if (levelId !== undefined)
      xml = xml.replace(
        /<levelId>[^<]*<\/levelId>/,
        `<levelId>${levelId}</levelId>`,
      );
    probe29_show(`probe29: sending "${label}"...`);
    const res = await probe29_post(xml);
    report.results.push({ label: label, levelId: levelId, ...res });
  }

  const text = JSON.stringify(report, null, 2);
  console.log("probe29 report\n" + text);
  const other = PROBE29_DESIGNS.find((d) => d !== report.designId);
  const pre = probe29_show(
    "probe29 done. Copy everything in this box into the PR.\n\n" + text,
  );
  if (other) {
    const a = document.createElement("a");
    a.href = `?designId=${other}&probe29=1`;
    a.textContent = `\n\nNext: run on design ${other}`;
    pre.appendChild(a);
  }
}

function probe29_start() {
  if (!params.get("probe29")) return;
  if (!user_id) {
    probe29_show("probe29: log in (top bar); the probe starts automatically.");
    setTimeout(probe29_start, 1000);
    return;
  }
  const n = PROBE29_VARIANTS.length;
  if (
    !confirm(
      `probe29 will save ${n} test designs named "i29 probe ..." to your account. Continue?`,
    )
  ) {
    probe29_show("probe29: cancelled.");
    return;
  }
  probe29_run();
}
