# fcsim

fcsim is the modern client for Fantastic Contraption.

# Build

## Requirements

First install the system requirements:

```sh
sh install_requirements.sh
```

This uses `apt` to install everything listed in `requirements.system`.

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

The build system used to use `ninja`, but `ninja` has been deprecated in favour of `scons`.

## Running the web build locally

To run the web build, you need to serve from the `html` directory. For example:

```sh
cd html
python3 -m http.server
```

Should get you a server at http://0.0.0.0:8000.

If you are having issues with the web build, try clearing your browser cache.

If you see a CORS issue, try enabling CORS everywhere.
There are certain browser extensions for this.

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

The regular build will produce the executable `stl_test`.

To see that the data structures are working correctly, you can use `valgrind` to check for memory leaks and other memory issues:

```sh
valgrind ./stl_test
```

## Memory

The web build also uses a custom `malloc`.

**If the game is crashing, try increasing `MALLOC_PAD_SPACE`.**

You can also check the memory usage using your browser developer tools.
This can be useful to diagnose memory leaks.

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