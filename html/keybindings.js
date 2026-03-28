"use strict";

// Keybindings displayed in the Keys menu.
// Sections appear in order; bindings appear in order within each section.
// Each binding: { key: "display text", description: "what it does" }

const KEYBINDINGS = [
  {
    section: "Pieces / Tools",
    bindings: [
      { key: "Click (+ drag)", description: "Place piece" },
      { key: "R", description: "Water rod" },
      { key: "S", description: "Solid rod" },
      { key: "U", description: "Unpowered wheel" },
      { key: "W", description: "Clockwise wheel" },
      { key: "C", description: "Counter-clockwise wheel" },
      { key: "M", description: "Grab (joint or component)" },
      { key: "D", description: "Delete (piece)" },
      { key: "Shift + Click", description: "Grab (ignores current tool)" },
      { key: "Ctrl + Click", description: "Delete (ignores current tool)" },
      { key: "Shift + Scroll Down", description: "Make fine adjustment finer" },
      { key: "Shift + Scroll Up", description: "Make fine adjustment coarser" },
    ],
  },
  {
    section: "Simulation",
    bindings: [{ key: "Space", description: "Start / stop simulation" }],
  },
  {
    section: "Speed",
    bindings: [
      { key: "1", description: "1x" },
      { key: "2", description: "2x" },
      { key: "3", description: "4x" },
      { key: "4", description: "10x" },
      { key: "5", description: "100x" },
      { key: "6", description: "1000x" },
      { key: "7", description: "10000x" },
      { key: "8", description: "100000x" },
      { key: "9", description: "MAX" },
      { key: "0", description: "Toggle 30 ↔ 36 TPS base" },
    ],
  },
  {
    section: "Camera",
    bindings: [
      { key: "Click + drag", description: "Pan camera" },
      { key: "Scroll", description: "Zoom camera" },
    ],
  },
];
