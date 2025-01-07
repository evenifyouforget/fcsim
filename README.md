# fcsim

fcsim is the modern client for Fantastic Contraption.

# Build

First install the system requirements:

```sh
sh install_requirements.sh
```

This uses `apt` to install everything listed in `requirements.system`.

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