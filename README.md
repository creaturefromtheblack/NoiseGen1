# NoiseGen1

A generative noise synthesizer and audio destruction processor for the Electro-Smith Daisy Seed platform.

NoiseGen1 combines 11 unique DSP noise engines into a single instrument. Each engine implements a different synthesis architecture, producing textures ranging from subtle crackles and drones to aggressive harsh noise walls, industrial grinding sounds, radio interference, metallic resonances, and chaotic digital artifacts.

Designed for experimental music, sound design, industrial, noise, ambient, and electroacoustic performance.

---

## Features

* 11 completely different noise synthesis engines
* Real-time patch switching
* External audio input processing
* Audio destruction and mangling algorithms
* RMS-based automatic level compensation
* Stereo output
* Optimized for live performance

### Controls

| Control  | Function                                                                     |
| -------- | ---------------------------------------------------------------------------- |
| Density  | Controls event density, triggering rate, chaos amount, and rhythmic activity |
| Register | Controls frequency range and spectral position                               |
| Input    | Controls the amount of external audio processing and destruction             |

---

## Included Noise Engines

The firmware contains a collection of specialized generators including:

* Deep subterranean drones
* Electrical crackles
* Harsh noise walls
* Industrial machine textures
* Radio interference simulations
* Metallic resonances
* Granular noise bursts
* Digital corruption artifacts
* Feedback-based structures
* Chaotic oscillators
* Hybrid destruction processors

Each engine is implemented with its own DSP architecture rather than simple parameter variations.

---

## Hardware Requirements

* Electro-Smith Daisy Seed
* Synthux Simple Fix (or compatible hardware)
* Audio input source (optional)
* Stereo audio output

---

## Building

### Requirements

* Arduino IDE
* DaisyDuino library

### Installation

1. Clone the repository:

```bash
git clone https://github.com/creaturefromtheblack/NoiseGen1.git
```

2. Open `NoiseGen1.ino` in Arduino IDE.

3. Install the DaisyDuino library.

4. Compile and upload to the Daisy Seed.

---

## Usage

Select one of the available engines using the hardware switch and shape the sound using the three control potentiometers.

External audio can be injected into the processing chain and transformed into heavily degraded, distorted, fragmented, or noise-infused textures.

---

## Sound Design Philosophy

NoiseGen1 was designed as a collection of dedicated noise instruments rather than a conventional synthesizer. The goal is to provide a broad palette of unstable, evolving, and often extreme sonic behaviors suitable for experimentation and performance.

---

## License

MIT License

Copyright (c) 2026 Creature from the Black

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
