"use strict";

// Keybindings displayed in the Keys menu.
// Sections appear in order; bindings appear in order within each section.
// Each binding: { key: "display text", description: "what it does" }

const KEYBINDINGS = [
  {
    section: "Tools",
    bindings: [
      { key: "R", description: "Rod tool" },
      { key: "S", description: "Solid rod tool" },
      { key: "U", description: "Unpowered wheel tool" },
      { key: "W", description: "Clockwise wheel tool" },
      { key: "C", description: "Counter-clockwise wheel tool" },
      { key: "M", description: "Move tool" },
      { key: "D", description: "Delete tool" },
      { key: "Shift (hold)", description: "Override current tool → Move" },
      { key: "Ctrl (hold)", description: "Override current tool → Delete" },
    ],
  },
  {
    section: "Simulation",
    bindings: [
      { key: "Space", description: "Start / stop simulation" },
    ],
  },
  {
    section: "Speed",
    bindings: [
      { key: "1 – 9", description: "Simulation speed preset (slowest → fastest)" },
      { key: "0", description: "Cycle base speed" },
    ],
  },
  {
    section: "Mouse",
    bindings: [
      { key: "Left drag", description: "Pan camera" },
      { key: "Scroll", description: "???" },
      { key: "Shift + Scroll", description: "???" },
      { key: "Right click", description: "???" },
    ],
  },
];
