import SCons.Builder
import subprocess
import json
import os
import time
import urllib.request


# Define a custom builder to generate the version.js file
def generate_version_js(target, source, env):
    """
    Generates a version.js file containing rich HTML for the version menu.
    Fetches info from Git and GitHub API.
    """
    build_timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
    github_token = os.environ.get("GITHUB_TOKEN")

    def run_git(args):
        try:
            return subprocess.check_output(["git"] + args).strip().decode("utf-8")
        except:
            return None

    # 1. Get Basic Git Info
    branch = run_git(["rev-parse", "--abbrev-ref", "HEAD"]) or "unknown"
    described_version = run_git(["describe", "--tags", "--always"]) or "v0.0.0"

    # 2. Extract GitHub Repo Info
    remote_url = run_git(["remote", "get-url", "origin"]) or ""
    repo_path = ""
    if "github.com" in remote_url:
        # Handles both https://github.com/user/repo and git@github.com:user/repo
        repo_path = (
            remote_url.split("github.com")[-1]
            .replace(":", "/")
            .replace(".git", "")
            .strip("/")
        )

    repo_url = f"https://github.com/{repo_path}" if repo_path else None

    # 3. Find PR (GitHub API)
    pr_link_html = ""
    if repo_path and branch != "main" and branch != "master":
        api_url = f"https://api.github.com/repos/{repo_path}/pulls?head={repo_path.split('/')[0]}:{branch}&state=open"
        try:
            req = urllib.request.Request(api_url)
            if github_token:
                req.add_header("Authorization", f"token {github_token}")

            with urllib.request.urlopen(req, timeout=5) as response:
                pulls = json.loads(response.read().decode())
                if pulls:
                    pr = pulls[0]
                    pr_link_html = f'<a href="{pr["html_url"]}" target="_blank" class="pr-badge">PR #{pr["number"]}</a>'
                else:
                    pr_link_html = '<span class="no-pr">No active PR</span>'
        except Exception as e:
            print(f"GitHub API Error: {e}")
            pr_link_html = '<span class="no-pr">PR status unavailable</span>'

    # 4. Get Commit Log since last tag
    changelog_html = "<ul><li>No recent commits found.</li></ul>"
    last_tag = run_git(["describe", "--tags", "--abbrev=0"])
    if last_tag:
        # Get up to 100 commits
        log_cmd = ["log", f"{last_tag}..HEAD", "--oneline", "-n", "100"]
        log_output = run_git(log_cmd)
        if log_output:
            lines = log_output.splitlines()
            items = []
            for line in lines:
                sha, msg = line.split(" ", 1)
                commit_url = f"{repo_url}/commit/{sha}" if repo_url else "#"
                items.append(
                    f'<li><a href="{commit_url}" target="_blank" class="commit-sha">{sha}</a> {msg}</li>'
                )
            changelog_html = f"<ul>{''.join(items)}</ul>"

    # 5. Build Final HTML String
    release_link = (
        f'<a href="{repo_url}/releases/tag/{last_tag}" target="_blank">{last_tag}</a>'
        if (repo_url and last_tag)
        else last_tag
    )

    html_payload = f"""
    <div class="version-content">
        <h3>Version Info</h3>
        <p><strong>Release:</strong> {release_link}</p>
        <p><strong>Branch:</strong> {branch} {pr_link_html}</p>
        <p><strong>Built:</strong> {build_timestamp}</p>

        <h4>Recent Commits (since {last_tag or "start"}):</h4>
        <div class="changelog-container">
            {changelog_html}
        </div>
    </div>
    """

    # Format the output as a JavaScript file
    js_content = f"const VERSION_HTML = {json.dumps(html_payload)};\n"
    js_content += f"function getVersionHtml() {{ return VERSION_HTML; }}"

    with open(target[0].abspath, "w") as f:
        f.write(js_content)

    return None


# source files
common_sources = [
    "src/arena.cpp",
    "src/arena_algorithm.cpp",
    "src/arena_graphics.cpp",
    "src/button.c",
    "src/export.c",
    "src/core.c",
    "src/gen.c",
    "src/graph.c",
    "src/graph_algorithm.cpp",
    "src/str.cpp",
    "src/text.cpp",
    "src/xml.c",
    "src/xoroshiro.cpp",
    "src/box2d/b2BlockAllocator.c",
    "src/box2d/b2Body.cpp",
    "src/box2d/b2BroadPhase.cpp",
    "src/box2d/b2CircleContact.c",
    "src/box2d/b2CollideCircle.cpp",
    "src/box2d/b2CollidePoly.cpp",
    "src/box2d/b2Contact.c",
    "src/box2d/b2ContactManager.cpp",
    "src/box2d/b2ContactSolver.cpp",
    "src/box2d/b2Island.cpp",
    "src/box2d/b2Joint.cpp",
    "src/box2d/b2PairManager.cpp",
    "src/box2d/b2PolyAndCircleContact.c",
    "src/box2d/b2PolyContact.c",
    "src/box2d/b2RevoluteJoint.cpp",
    "src/box2d/b2Settings.c",
    "src/box2d/b2Shape.cpp",
    "src/box2d/b2StackAllocator.c",
    "src/box2d/b2World.cpp",
    "src/fpmath/atan2.c",
    "src/fpmath/sincos.c",
    "src/fpmath/strtod.c",
]
stl_mock_sources = [
    "src/stl_mock.cpp",
]
test_sources = [
    "test/stl_test_main.cpp",
]
linux_sources = [
    "src/main.cpp",
    "src/timing.cpp",
    "src/fpmath/fpatan.s",
]
run_single_design_sources = [
    "src/run_single_design.cpp",
]
run_single_design_xml_sources = [
    "src/run_single_design_xml.cpp",
]
wasm_sources = [
    "src/arch/wasm/math.c",
    "src/arch/wasm/malloc.cpp",
    "src/arch/wasm/gl.c",
    "src/arch/wasm/string.c",
]

linux_sources_all = common_sources + linux_sources
run_single_design_sources_all = common_sources + run_single_design_sources
run_single_design_xml_sources_all = common_sources + run_single_design_xml_sources
test_sources_all = stl_mock_sources + test_sources
wasm_sources_all = common_sources + stl_mock_sources + wasm_sources

# flags
libs = [
    "X11",
    "GL",
    "pthread",
]
run_single_design_libs = [
    "pthread",
]

common_include = [
    "include",
]
wasm_include = [
    "arch/wasm/include",
]

test_defines = [
    "__wasm__",
]
fpatan_defines = [
    "USE_FPATAN",
]
run_single_defines = [
    "CLI",
]
msan_defines = [
    "SANITIZER_MSAN",
]

werror_ccflags = [
    # preemptively catch some other common errors
    "-Werror=uninitialized",
    "-Werror=return-type",
    "-Werror=sign-compare",
    "-Werror=format",
    "-Werror=strict-aliasing",
    "-Werror=unused-variable",
]
werror_cflags = [
    # actually caught an error originally in loop-contest
    "-Werror=incompatible-pointer-types",
    # preemptively catch some other common errors
    "-Werror=implicit-function-declaration",
    "-Werror=int-conversion",
    "-Werror=pointer-sign",
]
common_ccflags = werror_ccflags
linux_ccflags = [
    "-O3",
    "-flto",
]
asan_ccflags = [
    "-O1",
    "-g",
    "-fno-omit-frame-pointer",
    "-fsanitize=address,undefined",
]
asan_linkflags = [
    "-fsanitize=address,undefined",
]
msan_ccflags = [
    "-O1",
    "-g",
    "-fno-omit-frame-pointer",
    "-fsanitize=memory,undefined",
    "-fsanitize-memory-track-origins",
]
msan_linkflags = [
    "-fsanitize=memory,undefined",
]
cov_ccflags = [
    "-O1",
    "-g",
    "-fprofile-instr-generate",
    "-fcoverage-mapping",
]
cov_linkflags = [
    "-fprofile-instr-generate",
]
wasm_ccflags = [
    "-O2",
    "--target=wasm32",
    "-nostdlib",
    "-fno-rtti",
    "-fno-exceptions",
]

wasm_link_flags = [
    "--no-entry",
    "--export-all",
    "--allow-undefined",
]

# env
base_env = Environment(
    ENV={
        "PATH": [
            "/usr/bin",
            "/usr/lib/llvm-10/bin",
        ]
    },
    CFLAGS=werror_cflags,
)

linux_env = base_env.Clone(
    CCFLAGS=common_ccflags + linux_ccflags,
    CPPPATH=common_include,
    # if using CCPDEFINES, need to match for fpatan variant
    LIBS=libs,
)
linux_env.VariantDir("build/linux", ".", False)

run_single_design_env = base_env.Clone(
    CCFLAGS=common_ccflags + linux_ccflags,
    CPPPATH=common_include,
    CPPDEFINES=run_single_defines,
    LIBS=run_single_design_libs,
)
run_single_design_env.VariantDir("build/run_single_design", ".", False)

asan_env = base_env.Clone(
    CC="clang",
    CXX="clang++",
    CCFLAGS=common_ccflags + asan_ccflags,
    CPPPATH=common_include + wasm_include,
    CPPDEFINES=test_defines,
    LINKFLAGS=asan_linkflags,
)
asan_env.VariantDir("build/asan", ".", False)

msan_env = base_env.Clone(
    CC="clang",
    CXX="clang++",
    CCFLAGS=common_ccflags + msan_ccflags,
    CPPPATH=common_include + wasm_include,
    CPPDEFINES=test_defines + msan_defines,
    LINKFLAGS=msan_linkflags,
)
msan_env.VariantDir("build/msan", ".", False)

cov_env = base_env.Clone(
    CC="clang",
    CXX="clang++",
    CCFLAGS=common_ccflags + cov_ccflags,
    CPPPATH=common_include + wasm_include,
    CPPDEFINES=test_defines,
    LINKFLAGS=cov_linkflags,
)
cov_env.VariantDir("build/cov", ".", False)

wasm_env = base_env.Clone(
    CCFLAGS=common_ccflags + wasm_ccflags,
    CPPPATH=common_include + wasm_include,
    CPPDEFINES=["WASM_MEMORY_BACKEND"],
    CC="clang",
    CXX="clang++",
    LINK="wasm-ld",
    LINKFLAGS=wasm_link_flags,
)
wasm_env.VariantDir("build/wasm", ".", False)


# actual build
def build_with_variant(env, variant_dir, source_files, *args, **kwargs):
    source_files = [variant_dir + filename for filename in source_files]
    env.VariantDir(variant_dir, ".", True)
    env.Program(*args, source=source_files, **kwargs)


build_with_variant(linux_env, "build/linux/", linux_sources_all, target="fcsim")
build_with_variant(
    linux_env,
    "build/linux2/",
    linux_sources_all,
    target="fcsim-fpatan",
    CPPDEFINES=["USE_FPATAN"],
)
build_with_variant(
    run_single_design_env,
    "build/run_single_design/",
    run_single_design_sources_all,
    target="run_single_design",
)
build_with_variant(
    run_single_design_env,
    "build/run_single_design_xml/",
    run_single_design_xml_sources_all,
    target="run_single_design_xml",
)
build_with_variant(asan_env, "build/asan/", test_sources_all, target="stl_test_asan")
build_with_variant(msan_env, "build/msan/", test_sources_all, target="stl_test_msan")
build_with_variant(cov_env, "build/cov/", test_sources_all, target="stl_test_cov")
build_with_variant(wasm_env, "build/wasm/", wasm_sources_all, target="html/fcsim.wasm")

# Automated version tagging - build html/version.js
# Add the custom builder to the environment
js_env = Environment()
js_builder = SCons.Builder.Builder(action=generate_version_js)
js_env.Append(BUILDERS={"VersionJSBuilder": js_builder})

# Use the builder to generate the file
target = js_env.VersionJSBuilder("html/version.js", [])
js_env.AlwaysBuild(target)
