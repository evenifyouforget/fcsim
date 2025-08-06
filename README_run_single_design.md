# run_single_design

A command-line interface for running single Fantastic Contraption designs in fcsim.

## Usage

```bash
./run_single_design [max_ticks] < design.xml
```

- `max_ticks` (optional): Maximum number of ticks to simulate (default: 10000)
- Reads XML design data from stdin
- Outputs two lines to stdout:
  - Solve tick (or -1 if not solved)
  - End tick (total ticks simulated)

## Building

```bash
scons run_single_design
```

## Input Format

Accepts XML design files in the same format as Fantastic Contraption. The XML should contain a `retrieveLevel` or similar structure with:

- `levelBlocks`: Static geometry (rectangles, circles)
- `playerBlocks`: Dynamic pieces (wheels, rods, goal pieces)
- `start`: Build area definition
- `end`: Goal area definition

## Output Format

Compatible with ftlib's `run_single_design` output format:

```
<solve_tick>
<end_tick>
```

Where:
- `solve_tick`: Tick when the design solved (-1 if no solution)
- `end_tick`: Total ticks simulated

## Example

```bash
# Test a design that solves in 56 ticks
echo "<?xml version='1.0'?>..." | ./run_single_design 1000
```

Output:
```
56
56
```

This means the design solved at tick 56 and stopped early.