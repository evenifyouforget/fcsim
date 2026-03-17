# fcsim

fcsim is the modern client for Fantastic Contraption. [Play it here!](https://ft.jtai.dev/)

# Architecture

There are some aspects that are unlikely to change for the forseeable future.

* We assume GitHub; for GitHub Actions and for information to populate `version.js`.
* C/C++ is our main language, chosen for speed, compatibility with box2d, and ease of low level development for functions such as `atan2` and `malloc`.
* The web build (C/C++ to WASM) is our main product, and how most users will interact with fcsim. It has the most features. The native GUI still exists, though.

# Build

Currently, you can only build on Linux. It is actively tested on Linux Mint and GitHub Actions (using Ubuntu). We use `apt` to install dependencies, so Ubuntu or any distro based on Ubuntu should work out of the box. Otherwise, you will need to obtain the dependencies on your own.

## Requirements

First install the system requirements:

```sh
bash install_requirements.sh
```

This uses `apt` to install everything listed in `requirements.system`.

Before doing any work, you should switch to the venv:

```sh
source .venv/bin/activate
```

## Manually run linters

```sh
pre-commit run --all-files
```

This may cause some files to change.

## Actual build

Build everything:

```sh
scons
```

Clean:

```sh
scons -c
```

If you are having any build issues, try cleaning and then building.

The build system used to use `ninja`, but `ninja` has been removed in favour of `scons`.

## Running the web build locally

To run the web build, you need to serve from the `html` directory. For example:

```sh
cd html
python3 -m http.server
```

Should get you a server at http://0.0.0.0:8000.

If you are having issues with the web build, try clearing your browser cache.

CORS issues should not occur anymore due to an update to FC's servers, but if you see a CORS issue, try enabling CORS everywhere.
There are certain browser extensions for this.

## Multi-branch build

This is separate from the normal build, as it also builds other feature branches (meaning it is multiple times as expensive as the normal build). The actual web deployment uses this to populate the subdirectories.

```sh
bash build_subdirs.sh
```

To change the feature branches that will be built, or the directory names they will map to, edit `deploy.json`. If you are adding frozen build artifacts, you will need to add the files as well to the `v6-deploy` branch.

# `fcsim` vs `fcsim-fpatan` and others

The spectre level does not test `atan2`, so it is possible `794` (the target spectre) is actually several different spectres.

Considering our test machine to implement the true target spectre, Pawel discovered in 2025 that it uses certain x86 specialized instructions such as `fpatan`.
Such instructions are not available in web.

In light of this, we are making a distinction between `794-stable` and `794-target`:

* `794-stable` uses only the original software implementations of `atan2` and other math functions. The build `fcsim` uses this. It is 100% portable. It might not correspond to an actual spectre that exists in real FC.
* `794-target` is what `794` should actually be, according to our test machine. (Pawel's machine) It requires `fpatan`, `fsin`, and possibly other specialized instructions. It is only available on x86. It definitely corresponds to a real spectre that exists in real FC.

The build `fcsim-fpatan` has `fpatan` but is still missing `fsin` and possibly others, so it does not implement `794-target`.
Feel free to call it `794-fpatan` instead.

Hopefully at some point we can fully reverse engineer these specialized instructions and implement them in software, such that we can implement `794-target` in a fully portable way.

# Development

## Web STL

The web build does not support STL, so we need a mock STL.
The mock STL comes with basic tests.

The regular build will produce two test executables: `stl_test_asan`
(AddressSanitizer + UBSan) and `stl_test_msan` (MemorySanitizer + UBSan).

To run the full test suite covering both sanitizers:

```sh
pytest test/test_stl.py
```

## Memory

The web build also uses a custom `malloc`.

**If the game is crashing, try increasing `MALLOC_PAD_SPACE`.**

You can also check the memory usage using your browser developer tools.
This can be useful to diagnose memory leaks.

## Key Codes

Key event handlers use X11 key codes. You can find the code for a given key using `xev`:

```sh
xev
```

These scancodes (keycodes) are not to be confused with keysyms which appear as `XK_*` constants in X11 headers.

## Terminology and (eventual) data model

### Coordinates

+Y is down. +angle is clockwise.

### Units

Time: 1 tick.

Space: 1 mm (aka 1 unit).

### Designs

Levels and designs are only distinguished at the upper game layer. Internally, both share the underlying data model, and are referred to as designs. To differentiate it, the upper layer "designs" will be referred to as "blueprints".

A design contains:

* 1 build area, which is an Area
* 1 goal area, which is an Area
* 0 or more blocks
* An optional name (string) and description (string)

A level is just a design with the restriction that it cannot contain rods.

A blueprint additionally has an optional field for the level it is based on. How this works at the upper layer is that when a player opens a level A, and saves a blueprint B, B references A. This level field is preserved even if blueprint B is then resaved as design C - C will still reference A.

A design is alternately referred to as the initial state of the world.

### Areas

Contains:

* Center X (double)
* Center Y (double)
* Width (double)
* Height (double)

### Blocks

Contains:

* Index (Optional[int], implicit)
* Type ID (enum)
* Center X (double)
* Center Y (double)
* Width (double)
* Height (double)
* Rotation (double, radians)
* List of Joints (implicit)
* List of Optional[Joint Edge] for each Joint

Internally, used during a session and never exported, it is also useful to have:

* Unique ID (int)

While all of this works without the unique ID, it helps performance if we don't need to constantly rename blocks (recalculate indices). Unique IDs have the invariant that higher IDs are newer - or in the case of loading XML, higher IDs correspond to blocks that appear later in the file. Unique ID ordering is a stricter version of index ordering, and there will never be a case where index_i < index_j and uid_i > uid_j. Furthermore, internally, it is helpful to not store indices, and only calculate indices on export.

Blocks are also referred to as pieces, though pieces can also mean player blocks specifically.

Type IDs have some notable partitions.

#### Type ID partition: level, design

Type IDs can be partitioned into:

* level blocks
* design blocks

Design blocks can be further partitioned into:

* player blocks
* goal blocks

In the blueprint editor, only player blocks can be added or deleted. The design solves if all goal blocks are inside the goal area at the same time.

#### Type ID partition: rectangle, circle

Type IDs can be partitioned into:

* rectangle
* circle

These behave noticeably differently in physics.

For circles, the height does not matter, and the width is treated as the diameter. For normalization reasons, we may require the height is equal to the width, but original FC is lenient about this.

For circles, there is some inconsistency in the XML format: static circles and dynamic circles use radius instead of diameter.

For rods (which are a subset of rectangle), the height is fixed based on the type ID.

#### Indexing

Level blocks do not use indices.

Design blocks are automatically assigned indices based on the order that they appear: the first design block is 0, the next is 1, and so on. Manually assigned indices in XML are ignored.

#### Joints

The number of joints is dependent on the type ID, and in some cases, dependent on the size.

* For rods (water rod, wood rod), it is 2 (left side, and right side)
* For goal rectangles, it is 5 (center, and 4 corners)
* For design circles (the 3 wheel types, and goal circles), it is 5 (center, and 4 cardinal points) or 9 (additional 4 cardinal points at fixed diameter of 40mm if the block's actual diameter is greater than 40mm)
* For all other type IDs, it is 0

Notably, level blocks always have 0 joints.

These joints appear in a fixed order (Though the correct order has only been documented for rods. We genuinely don't know for every other type)

The canonical ID of a joint is the tuple (block index, joint index within block). However, using unique IDs instead, this is (block unique ID, joint index within block).

XML of joint edges only specifies the block index, and is ambiguous about which joint within the block it corresponds to. The code uses closest matching, and in rare cases this results in a rod re-jointing bug. Internally we specify the joint index within the block to avoid such issues, but on export we are forced to use the ambiguous format.

#### Joint Edges

Blocks can be connected to other blocks via joints. The smallest unit of this is 1 joint connecting to 1 other joint. This is called a joint edge.

Joint edges are a directional relationship: A -> B is different than B -> A.

It is always guaranteed that if (A_block, A_joint) -> (B_block, B_joint), then A_block.index > B_block.index and A_block.uid > B_block.uid.

Joint edges are owned by the outgoing side. So if (A_block, A_joint) -> (B_block, B_joint), then this joint edge is stored on A_block. In the raw data, A_block will know about its outgoing edge, but B_block will not know about its incoming edge.

A joint can have at most 1 incoming joint edge, and at most 1 outgoing joint edge. This results in a linear graph structure.

Both joints involved in a joint edge will have "the same" position, however, due to floating-point math, the actual calculated coordinates may be slightly different.

#### Joint Stack

A joint and all other joints connected to it by joint edges is called a joint stack. This is what the user perceives as a single joint on screen. The order does matter for physics.

When adding a new joint A to an existing joint stack {B, C, ...}, the joint edge will connect A to whichever joint in the joint stack does not already have an incoming edge. Equivalently, the block with the highest index or highest unique ID is chosen.

#### Regarding rods and migration work

The old data model has rods specified by their two joint endpoints rather than the usual position, size, and angle. It seemed convenient at first, but it's really complicating the design not to have a unified model where the position, size, and angle is the source of truth, and blocks always own their joints.

### Collision

Most blocks collide with most other blocks. There are some particular cases where block A will not collide with block B:

* A and B share a joint stack
* At least one of A and B is a water rod, and both A and B are design blocks

A design that has any player blocks colliding is illegal.

### Build Area

A design that has any player blocks not fully contained in the build area is illegal. However, in some cases, level creators artistically violate this, and the game permits it - if the starting state has wheels outside the build area, the blueprint can still have more player blocks added, as long as it does not become "more illegal".

### Box2D World

A design is instantiated into a world. The same design can potentially be instantiated into multiple worlds, which will all simulate independently.

When creating the world, each block is translated into a body. The body has properties depending on the block's type ID.

Internally, each block currently stores a pointer to its corresponding body, though we are considering decoupling this as there's no real reason a block should own its body.

A world can have its own time progress in ticks. It progresses 1 tick at a time. This is independent of other worlds.

### Editing

There are 3 types of edits which can be performed:

* Deleting a block
* Moving a joint or a connected component
* Creating a block

Deleting a block is atomic in the sense that no state is required to be maintained to respond to further user inputs. The other 2 are continuous.

Continuous edit types should be transactional in the sense that it is possible to cancel the operation and revert to the state before the edit started.

Creating a block can be considered to be a fusion of adding a block and dragging a joint (for rods, the right side; for all others, the center joint), with the special behaviour that adding a block is the only time a new joint edge can be created, and it must be from the newly added block to an existing block. Depending on implementation, having block creation share code with move operations may or may not be useful.

#### Move Operation State

There are a few sets relevant to a move operation (including creating a block). All of them can contain both blocks and joints.

* The rigid set
* The affected set

Suppose (dx, dy) is the difference by which the mouse has moved, scaled to world space and with any further scale modifiers already applied.

The rigid set contains all blocks and joints which will be translated by (dx, dy) without any change in size or rotation. It is equal to its own closure under:

* Adding all joints that share a joint stack with a joint already in the set
* Adding a block that owns a joint already in the set, if that block is not a rod
* Adding all joints owned by a block already in the set

The affected set is the union of the rigid set, and all blocks owning a joint which is in the rigid set. Alternatively, the affected set is, in the general case, the set of all blocks and joints whose position, size, or angle may need to be recalculated as a result of the movement.

# Performance

fcsim promised to be fast. So how fast is it?

Here are some measurements from the dev's local machine.

| Level/design              | At rest? | Platform     | Ticks per second | Speed up (compared to 30 TPS) |
|---------------------------|----------|--------------|------------------|-------------------------------|
| poocs (level 634052)      | yes      | Linux native | 18457892         | 615263                        |
| poocs (level 634052)      | yes      | Web          | 7089081          | 236303                        |
| Galois (design 12668445)  | no       | Web          | 7646             | 255                           |
| Galois (design 12668445)  | yes      | Web          | 99731            | 3324                          |
| Reach Up (design 5371157) | no       | Web          | 78209            | 2607                          |

See [#2](https://github.com/evenifyouforget/fcsim/pull/2) and [#9](https://github.com/evenifyouforget/fcsim/pull/9) for full commentary.
