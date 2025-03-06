# fcsim

fcsim is the modern client for Fantastic Contraption.

# Build

## Requirements

First install the system requirements:

```sh
sh install_requirements.sh
```

This uses `apt` to install everything listed in `requirements.system`.

On older systems, some files may not be in the right location. You can try running:

```sh
source path_fix.sh
```

This applies some automatic fixes.

## Actual build

Build everything:

```sh
ninja
```

## Build Linux client

```sh
ninja fcsim
```

## Build web client

```sh
ninja html/fcsim.wasm
```

# Development

## Web STL

The web build does not support STL, so we need a mock STL.
The mock STL comes with basic tests.

You can build the mock STL tests with:

```sh
ninja stl_test
```

Which will produce the executable `stl_test`.

To see that the data structures are working correctly, you can use `valgrind` to check for memory leaks and other memory issues:

```sh
valgrind ./stl_test
```

# Performance

fcsim promised to be fast. So how fast is it?

Here are some measurements from the dev's local machine.

| Level/design              | At rest? | Platform     | Ticks per second | Speed up (compared to 30 TPS) |
|---------------------------|----------|--------------|------------------|-------------------------------|
| poocs (level 634052)      | yes      | Linux native | 7807170          | 260239                        |
| poocs (level 634052)      | yes      | Web          | 6561002          | 218700                        |
| Galois (design 12668445)  | no       | Web          | 3330             | 111                           |
| Galois (design 12668445)  | yes      | Web          | 92861            | 3095                          |
| Reach Up (design 5371157) | no       | Web          | 80831            | 2694                          |

See [#2](https://github.com/evenifyouforget/fcsim/pull/2) for full commentary.