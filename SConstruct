import SCons.Builder
import subprocess
import json
import os
import time

# Define a custom builder to generate the version.js file
def generate_version_js(target, source, env):
    """
    Generates a version.js file containing build and Git information.
    The output structure is: 
    [branch (string), uncommitted_lines (int), described_version (string), build_timestamp (int)]
    """
    build_timestamp = int(time.time()) 

    try:
        # Get Git information
        branch = subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).strip().decode('utf-8')
        described_version = subprocess.check_output(['git', 'describe', '--tags', '--always']).strip().decode('utf-8')
        uncommitted_lines = 0
        diff_output = subprocess.check_output(['git', 'diff', '--numstat']).strip().decode('utf-8')
        
        if diff_output:
            for line in diff_output.splitlines():
                # Format: <added>\t<deleted>\t<filename>
                parts = line.split()
                if len(parts) >= 2 and parts[0].isdigit() and parts[1].isdigit():
                    uncommitted_lines += int(parts[0]) + int(parts[1])

        version_info = [
            branch,
            uncommitted_lines, 
            described_version,
            build_timestamp
        ]

        # Format the output as a JavaScript function
        js_content = f'function getVersionInfo() {{\n  return {json.dumps(version_info)};\n}}'

        # Write to the target file
        with open(target[0].abspath, 'w') as f:
            f.write(js_content)

        print(f"Generated {target[0].name} with version info from git.")
    except (subprocess.CalledProcessError, FileNotFoundError) as e:
        print(f"Error getting git information: {e}")
        # Create a fallback file to prevent build failure
        with open(target[0].abspath, 'w') as f:
            f.write(f'function getVersionInfo() {{ return ["unknown", 0, "unknown-version", {build_timestamp}]; }}')
        print(f"Generated a fallback {target[0].name} file.")
    return None

# source files
common_sources = [
    "src/arena.c",
    "src/arena_algorithm.cpp",
    "src/arena_graphics.cpp",
    "src/button.c",
    "src/export.c",
    "src/core.c",
    "src/gen.c",
    "src/graph.c",
    "src/graph_algorithm.cpp",
    "src/str.c",
    "src/text.c",
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
    "src/arch/wasm/malloc.c",
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
test_ccflags = [
    "-O3",
    "-flto",
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
    ENV = {
        'PATH': [
            '/usr/bin',
            '/usr/lib/llvm-10/bin',
            ]
        },
    CFLAGS = werror_cflags
    )

linux_env = base_env.Clone(
    CCFLAGS = common_ccflags + linux_ccflags,
    CPPPATH = common_include,
    # if using CCPDEFINES, need to match for fpatan variant
    LIBS = libs
    )
linux_env.VariantDir("build/linux", ".", False)

run_single_design_env = base_env.Clone(
    CCFLAGS = common_ccflags + linux_ccflags,
    CPPPATH = common_include,
    CPPDEFINES = run_single_defines,
    LIBS = run_single_design_libs
    )
run_single_design_env.VariantDir("build/run_single_design", ".", False)

test_env = base_env.Clone(
    CCFLAGS = common_ccflags + test_ccflags,
    CPPPATH = common_include + wasm_include,
    CPPDEFINES = test_defines
    )
test_env.VariantDir("build/test", ".", False)

wasm_env = base_env.Clone(
    CCFLAGS = common_ccflags + wasm_ccflags,
    CPPPATH = common_include + wasm_include,
    CC = 'clang',
    CXX = 'clang++',
    LINK = 'wasm-ld',
    LINKFLAGS = wasm_link_flags
    )
wasm_env.VariantDir("build/wasm", ".", False)

# actual build
def build_with_variant(env, variant_dir, source_files, *args, **kwargs):
    source_files = [variant_dir + filename for filename in source_files]
    env.VariantDir(variant_dir, ".", True)
    env.Program(*args, source = source_files, **kwargs)
build_with_variant(linux_env, "build/linux/", linux_sources_all, target = 'fcsim')
build_with_variant(linux_env, "build/linux2/", linux_sources_all, target = 'fcsim-fpatan', CPPDEFINES = ['USE_FPATAN'])
build_with_variant(run_single_design_env, "build/run_single_design/", run_single_design_sources_all, target = 'run_single_design')
build_with_variant(run_single_design_env, "build/run_single_design_xml/", run_single_design_xml_sources_all, target = 'run_single_design_xml')
build_with_variant(test_env, "build/test/", test_sources_all, target = 'stl_test')
build_with_variant(wasm_env, "build/wasm/", wasm_sources_all, target = 'html/fcsim.wasm')

# Automated version tagging - build html/version.js
# Add the custom builder to the environment
js_env = Environment()
js_builder = SCons.Builder.Builder(action=generate_version_js)
js_env.Append(BUILDERS={'VersionJSBuilder': js_builder})

# Use the builder to generate the file
js_env.VersionJSBuilder('html/version.js', [])