# mock spectres
AddOption(
    '--autorun',
    dest='autorun',
    type='int',
    action='store',
    default=0,
    help='Causes native game to start running immediately at max speed, print solve time, and quit after reaching tick limit'
    )
AddOption(
    '--make-atan2-wrong',
    dest='make_atan2_wrong',
    type='int',
    action='store',
    default=0,
    help='Make atan2 wrong on purpose about 2^-N of the time. (random seed) Useful to test spectre levels for sufficient sensitivity.')

# source files
common_sources = [
    "src/arena.c",
    "src/arena_graphics.cpp",
    "src/button.c",
    "src/export.c",
    "src/core.c",
    "src/gen.c",
    "src/graph.c",
    "src/str.c",
    "src/text.c",
    "src/xml.c",
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
wasm_sources = [
    "src/arch/wasm/math.c",
    "src/arch/wasm/malloc.c",
    "src/arch/wasm/gl.c",
    "src/arch/wasm/string.c",
    ]

linux_sources_all = common_sources + linux_sources
test_sources_all = stl_mock_sources + test_sources
wasm_sources_all = common_sources + stl_mock_sources + wasm_sources

# flags
libs = [
    "X11",
    "GL",
    "pthread",
    ]

common_include = [
    "include",
    ]
wasm_include = [
    "arch/wasm/include",
    ]

common_defines = []
linux_defines = []
test_defines = [
    "__wasm__",
    ]
fpatan_defines = [
    "USE_FPATAN",
    ]
wasm_defines = []

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

# autorun
autorun = GetOption('autorun')
if autorun:
    common_defines.append(f'AUTORUN={autorun}')
    print(f'Using autorun with tick limit {autorun}')

# fake bad math
def get_random_mask(n):
    mask_bits = 0
    for offset in random.sample(range(64), n):
        mask_bits |= 1 << offset
    return mask_bits
make_atan2_wrong = GetOption('make_atan2_wrong')
if make_atan2_wrong:
    import random
    k = max(0, make_atan2_wrong - 64)
    k = random.randint(k, make_atan2_wrong - k)
    mask_bits1 = get_random_mask(k)
    mask_bits2 = get_random_mask(make_atan2_wrong - k)
    premask_xor1 = random.getrandbits(64)
    premask_xor2 = random.getrandbits(64)
    common_defines.append(f'MAKE_ATAN2_WRONG_MASK_Y={mask_bits1}ull')
    common_defines.append(f'MAKE_ATAN2_WRONG_MASK_X={mask_bits2}ull')
    common_defines.append(f'MAKE_ATAN2_WRONG_XOR_Y={premask_xor1}ull')
    common_defines.append(f'MAKE_ATAN2_WRONG_XOR_X={premask_xor2}ull')
    print(f'Using atan2 error level {make_atan2_wrong} with mask = {mask_bits1:x} {mask_bits2:x}, xor = {premask_xor1:x} {premask_xor2:x}')

# env
base_env = Environment(
    ENV = {
        'PATH': [
            '/usr/bin',
            '/usr/lib/llvm-10/bin',
            ]
        }
    )

linux_env = base_env.Clone(
    CCFLAGS = linux_ccflags,
    CPPPATH = common_include,
    CPPDEFINES = common_defines + linux_defines,
    LIBS = libs
    )
linux_env.VariantDir("build/linux", ".", False)

test_env = base_env.Clone(
    CCFLAGS = test_ccflags,
    CPPPATH = common_include + wasm_include,
    CPPDEFINES = common_defines + test_defines,
    )
test_env.VariantDir("build/test", ".", False)

wasm_env = base_env.Clone(
    CCFLAGS = wasm_ccflags,
    CPPPATH = common_include + wasm_include,
    CPPDEFINES = common_defines + wasm_defines,
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
build_with_variant(linux_env, "build/linux2/", linux_sources_all, target = 'fcsim-fpatan', CPPDEFINES = common_defines + linux_defines + ['USE_FPATAN'])
build_with_variant(test_env, "build/test/", test_sources_all, target = 'stl_test')
build_with_variant(wasm_env, "build/wasm/", wasm_sources_all, target = 'html/fcsim.wasm')