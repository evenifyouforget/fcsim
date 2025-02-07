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
ninja stl test
```

Which will produce the executable `stl_test`.

To see that the data structures are working correctly, you can use `valgrind` to check for memory leaks and other memory issues:

```sh
valgrind ./stl_test
```
