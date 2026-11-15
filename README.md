# Maze Quest — Tilt-Controlled Maze Game on a PIC24 Microcontroller

Tilt the board to roll a ball through a maze and reach the finish before the countdown ends. Maze Quest runs as bare-metal C firmware on a **Microchip PIC24FJ256GA705** and was my final project for the *Introduction to Embedded Systems* course at Shenkar College (2025).

---

## Features

- **Tilt control.** An **ADXL345 accelerometer** is read over **I2C**. The board's tilt sets the ball's movement on the X and Y axes, scaled by the difficulty level.
- **Scrolling maze.** The **96×96 OLED** display (driven over **SPI**) shows a window into a maze twice the height of the screen. A **potentiometer**, read with the **12-bit ADC**, scrolls the window up and down.
- **Four mazes and three difficulty levels.**

  | Level | Time limit | Ball speed |
  | --- | --- | --- |
  | 1 | 40 s | ×0.8 |
  | 2 | 30 s | ×1.0 |
  | 3 | 20 s | ×1.2 |

- **Countdown in tenths of a second.** A **100 ms Timer1 interrupt** updates the on-screen timer, makes the display blink (inverse mode) during the last 5 seconds and ends the game when time runs out.
- **Wall collisions.** Walls are stored as horizontal and vertical segments. Collisions are checked separately on each axis, so the ball slides along a wall instead of sticking to it.
- **Leaderboard.** The winner enters a name letter by letter, choosing each letter (A–Z) with the potentiometer, and the top 3 scores are kept. The score is the percentage of time left.
- **Emergency exit.** Holding **S1** for 3 seconds quits the current game, detected inside the timer interrupt.

## How it works

| Part | Details |
| --- | --- |
| Start-up | Configures GPIO directions, the ADC (12-bit mode) and Timer1 directly through their registers. Checks the accelerometer's device ID (`0xE5`) and stops with an error message on the screen if the sensor is missing or the I2C bus fails. |
| Game loop | Reads the accelerometer and potentiometer, computes the ball's next position, checks every wall for collisions, redraws only what changed and checks for the finish. |
| Timer interrupt | Keeps game time, draws the countdown, blinks the display in the last 5 seconds and detects the long press on S1. |
| Shared display | The interrupt and the game loop both draw on the OLED, so the timer interrupt is paused while the maze is redrawn during scrolling. This prevents corrupted frames. |

## Hardware

- PIC24FJ256GA705 development board (course kit)
- ADXL345 3-axis accelerometer (I2C)
- 96×96 color OLED display (SPI)
- Potentiometer on an analog input, push buttons S1 and S2

## Project structure

```
PIC24Maze.X/
├── main.c              # Game logic, Timer1 interrupt, ADC, menus, collisions, leaderboard (written by me)
├── instructions.txt    # User manual: controls, levels and scoring
├── System/             # Clock and system initialization (MCC-generated)
├── i2cDriver/          # I2C1 driver (Microchip)
├── spiDriver/          # SPI1 driver (Microchip)
├── oledDriver/         # OLED driver and drawing helpers (Microchip)
├── accelDriver/        # Accelerometer I2C helper (course template)
└── nbproject/          # MPLAB X project configuration
```

The game itself, `main.c`, is my work. The peripheral drivers come from Microchip and from the course template.

## Build and run

1. Install **MPLAB X IDE** (built with v6.25) and the **XC16** compiler (v2.10).
2. Open the `PIC24Maze.X` folder as a project in MPLAB X.
3. Build the project, then program the board with your debugger or programmer.

## Controls

| Action | Control |
| --- | --- |
| Move to the next menu option | Press S1 |
| Confirm the selected option | Press S2 |
| Move the ball | Tilt the board |
| Scroll the maze | Rotate the potentiometer |
| Enter your name | Rotate the potentiometer to pick a letter; S1 next letter, S2 previous letter, both buttons to save |
| Quit the game | Hold S1 for 3 seconds |

The user manual handed in with the project is in [`PIC24Maze.X/instructions.txt`](PIC24Maze.X/instructions.txt).

## Author

**Atheer Azaizeh** · [github.com/AtheerAzaizeh](https://github.com/AtheerAzaizeh)
