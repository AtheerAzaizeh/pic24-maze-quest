/*
 * File:   main.c
 * Author: Atheer Azaiza
 *
 * Created on July, 2025
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "System/system.h"
#include "System/delay.h"
#include "oledDriver/oledC.h"
#include "oledDriver/oledC_colors.h"
#include "oledDriver/oledC_shapes.h"

#include "i2cDriver/i2c1_driver.h"
#include "accelDriver/Accel_i2c.h"

#define ALPHABET_SIZE 26
#define FIRST_LETTER 'A'
#define LAST_LETTER 'Z'
#define TOP_SCORES_COUNT 3

static int gameRunning = 1;
static int timeCounter = 0;
static int levelDuration = 40;
static int playerPoints = 1;
static int displayInvert = 1;
static int btn1Duration = 0;
static bool gameExited = 0;

typedef struct
{
    int posX;
    int posY;
} Player;

typedef struct
{
    int direction; // 0 if horizontal, 1 if vertical
    int posX;      // start x position
    int posY;      // start y position
    int size;      // length of the wall
} Barrier;

typedef struct
{
    Barrier barriers[50];
    int barrierCount;
    int exitX;
    int exitY;
} Level;

typedef struct
{
    int lastY;
    int currentY;
    int height;
} Camera;

typedef struct
{
    char playerName[7];
    int finalScore;
    int levelNumber;
} ScoreEntry;

static ScoreEntry topScores[TOP_SCORES_COUNT];

void SystemSetup(void)
{
    // Initialize LED/Switch IO Direction (TRISx)
    TRISA |= (1 << 11); // S1 button
    TRISA |= (1 << 12); // S2 button
    TRISA &= ~(1 << 8);
    TRISA &= ~(1 << 9);

    // Set RB12 (AN8) as Analog Input
    TRISB |= (1 << 12);
    ANSB |= (1 << 12);

    // Initialize A/D Circuit (AD1CON1)
    AD1CHS = 8;
    AD1CON1 = 0x00; // Clear all bits
    AD1CON1bits.SSRC = 0;
    AD1CON1bits.FORM = 0;
    AD1CON1bits.MODE12 = 1;
    AD1CON1bits.ADON = 1;

    // Configure AD1CON2
    AD1CON2 = 0;

    // Configure AD1CON3
    AD1CON3 = 0x00; // Clear all bits
    AD1CON3bits.ADCS = 0xFF;
    AD1CON3bits.SAMC = 0x10;

    //
    oledC_setBackground(OLEDC_COLOR_BLACK);
    oledC_clearScreen();
}

void ClockSetup(void)
{
    T1CONbits.TON = 1;
    T1CONbits.TSIDL = 1;
    T1CONbits.TGATE = 0;
    T1CONbits.TCKPS = 2;
    T1CONbits.TCS = 0;
    INTCON2bits.GIE = 1;
    PR1 = 6250; // Changed for 100ms interrupts (10Hz)
    IPC0bits.T1IP = 1;
    IEC0bits.T1IE = 1;
    IFS0bits.T1IF = 0;
    return;
}

void fatalError(char *message)
{
    oledC_DrawString(0, 20, 2, 2, message, OLEDC_COLOR_DARKRED);
    for (;;)
        ;
}

I2Cerror AccelSetup(I2Cerror status)
{
    unsigned char deviceId;

    i2c1_open();
    status = i2cReadSlaveRegister(0x3A, 0, &deviceId);

    if (status == OK)
        if (deviceId == 0xE5)
        {
            oledC_DrawString(10, 10, 2, 2, (uint8_t *)"ADXL345", OLEDC_COLOR_BLACK);
        }
        else
            fatalError("Acc!Found");
    else
        fatalError("I2C Error");

    status = i2cWriteSlave(0x3A, 0x2D, 8);

    return status;
}

void __attribute__((__interrupt__)) _T1Interrupt(void)
{
    char timeDisplay[10] = "        ";
    timeCounter++;
    int remainingTime = levelDuration * 10 - timeCounter; // Convert to deciseconds
    int seconds = remainingTime / 10;
    int deciseconds = remainingTime % 10;
    sprintf(timeDisplay, "%d.%d", seconds, deciseconds);
    oledC_DrawRectangle(0, 0, 40, 20, OLEDC_COLOR_BLACK);
    oledC_DrawString(2, 2, 1, 1, timeDisplay, OLEDC_COLOR_RED);

    if (remainingTime < 50) // Less than 5 seconds
    {
        if (displayInvert)
        {
            oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_INVERSE, NULL, 0);
            displayInvert = 0;
        }
        else
        {
            oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_ON, NULL, 0);
            displayInvert = 1;
        }
    }

    if (timeCounter >= levelDuration * 10)
    {
        gameRunning = 0; // End the game
        oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_ON, NULL, 0);
        displayInvert = 1;
    }

    if (!(PORTA & (1 << 11)))
    { // if S1 is pressed
        btn1Duration++;
        if (btn1Duration >= 30)
        {                    // 3 seconds when timer interrupt is 100ms
            gameRunning = 0; // or any other flag to indicate exit
            gameExited = 1;
        }
    }
    else
    {
        btn1Duration = 0;
    }

    IFS0bits.T1IF = 0; // Clear the Timer1 interrupt flag
}

void savePlayerScore(char *playerName, int levelChoice)
{
    oledC_clearScreen();

    // Calculate percentage score
    int maxPossibleScore = levelDuration * 10; // Maximum time in deciseconds
    int timeUsed = timeCounter;
    int percentageScore = ((maxPossibleScore - timeUsed) * 100) / maxPossibleScore;
    if (percentageScore < 0)
        percentageScore = 0;

    // Check if score qualifies for top 3
    if (percentageScore <= topScores[2].finalScore && topScores[2].finalScore > 0)
    {
        return; // Skip saving if score is lower than 3rd place
    }

    int insertPos;
    for (insertPos = 0; insertPos < TOP_SCORES_COUNT; insertPos++)
    {
        if (percentageScore > topScores[insertPos].finalScore)
        {
            break;
        }
    }

    // Shift scores down to make room for the new score
    for (int j = TOP_SCORES_COUNT - 1; j > insertPos; j--)
    {
        topScores[j] = topScores[j - 1];
    }

    // Store the new score
    strncpy(topScores[insertPos].playerName, playerName, sizeof(topScores[insertPos].playerName) - 1);
    topScores[insertPos].playerName[sizeof(topScores[insertPos].playerName) - 1] = '\0';
    topScores[insertPos].finalScore = percentageScore;
    topScores[insertPos].levelNumber = levelChoice;
}

void showLeaderboard()
{
    // Draw border with '*' and green color
    for (int i = 0; i < 96; i += 8)
    {
        oledC_DrawString(0, i, 1, 1, "*", OLEDC_COLOR_GREEN);
        oledC_DrawString(90, i, 1, 1, "*", OLEDC_COLOR_GREEN);
    }
    for (int i = 0; i < 96; i += 6)
    {
        oledC_DrawString(i, 0, 1, 1, "*", OLEDC_COLOR_GREEN);
        oledC_DrawString(i, 90, 1, 1, "*", OLEDC_COLOR_GREEN);
    }

    oledC_DrawString(30, 14, 1, 1, "Victory!", OLEDC_COLOR_GREEN);
    for (int i = 0; i < TOP_SCORES_COUNT; i++)
    {
        char scoreText[30];
        sprintf(scoreText, "%s: %d%% L:%d", topScores[i].playerName, topScores[i].finalScore, topScores[i].levelNumber);
        oledC_DrawString(8, (i + 1) * 15 + 20, 1, 1, scoreText, OLEDC_COLOR_WHITE);
    }
    oledC_DrawString(8, 75, 1, 1, "S1 to restart!", OLEDC_COLOR_CYAN);
}

void showGameOver()
{
    // Draw border with '*' and red color
    for (int i = 0; i < 96; i += 8)
    {
        oledC_DrawString(0, i, 1, 1, "*", OLEDC_COLOR_RED);
        oledC_DrawString(90, i, 1, 1, "*", OLEDC_COLOR_RED);
    }
    for (int i = 0; i < 96; i += 6)
    {
        oledC_DrawString(i, 0, 1, 1, "*", OLEDC_COLOR_RED);
        oledC_DrawString(i, 90, 1, 1, "*", OLEDC_COLOR_RED);
    }

    oledC_DrawString(30, 30, 1, 1, "Game Over!", OLEDC_COLOR_RED);
    oledC_DrawString(8, 50, 1, 1, "Time's up!", OLEDC_COLOR_WHITE);
    oledC_DrawString(8, 70, 1, 1, "S1 to restart!", OLEDC_COLOR_CYAN);
}

int detectButton1()
{
    static int btnState = 0; // false
    if ((PORTA & (1 << 11)) == 0)
    {
        btnState = 1; // true
    }
    else
    {
        btnState = 0;
    }

    return btnState; // return the button state
}

int detectButton2()
{
    static int btnState = 0; // false
    if ((PORTA & (1 << 12)) == 0)
    {
        btnState = 1; // true
    }
    else
    {
        btnState = 0;
    }

    return btnState; // return the button state
}

int readPotentiometer()
{
    AD1CON1bits.SAMP = 1; // start sampling
    DELAY_milliseconds(65);
    AD1CON1bits.SAMP = 0; // end sampling and start conversion
    while (!AD1CON1bits.DONE)
        ;            // wait for the conversion to complete
    return ADC1BUF0; // return the result
}

int selectDifficulty()
{
    int difficultyLevel = 1;
    oledC_DrawString(2, 2, 1, 1, "Difficulty:", OLEDC_COLOR_CORAL);
    oledC_DrawString(32, 16, 1, 1, "Easy", OLEDC_COLOR_RED);
    oledC_DrawString(32, 32, 1, 1, "Medium", OLEDC_COLOR_WHITE);
    oledC_DrawString(32, 48, 1, 1, "Hard", OLEDC_COLOR_WHITE);
    while (!((PORTA & (1 << 12)) == 0))
    { // exit when button 2 is pressed

        if ((PORTA & (1 << 11)) == 0)
        { // if button 1 is pressed
            difficultyLevel++;
            oledC_DrawString(32, 16, 1, 1, "Easy", OLEDC_COLOR_BLACK);
            oledC_DrawString(32, 32, 1, 1, "Medium", OLEDC_COLOR_BLACK);
            oledC_DrawString(32, 48, 1, 1, "Hard", OLEDC_COLOR_BLACK);
            if (difficultyLevel == 4)
                difficultyLevel = 1;
            if (difficultyLevel == 1)
            {
                oledC_DrawString(32, 16, 1, 1, "Easy", OLEDC_COLOR_RED);
                oledC_DrawString(32, 32, 1, 1, "Medium", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 48, 1, 1, "Hard", OLEDC_COLOR_WHITE);
            }
            else if (difficultyLevel == 2)
            {
                oledC_DrawString(32, 16, 1, 1, "Easy", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 32, 1, 1, "Medium", OLEDC_COLOR_RED);
                oledC_DrawString(32, 48, 1, 1, "Hard", OLEDC_COLOR_WHITE);
            }
            else if (difficultyLevel == 3)
            {
                oledC_DrawString(32, 16, 1, 1, "Easy", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 32, 1, 1, "Medium", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 48, 1, 1, "Hard", OLEDC_COLOR_RED);
            }
        }
    }
    oledC_clearScreen();
    return difficultyLevel;
}

int selectLevel(void)
{
    int levelChoice = 1;
    oledC_DrawString(2, 2, 1, 1, "Choose Level:", OLEDC_COLOR_CORAL);
    oledC_DrawString(32, 12, 1, 1, "Level 1", OLEDC_COLOR_RED);
    oledC_DrawString(32, 28, 1, 1, "Level 2", OLEDC_COLOR_WHITE);
    oledC_DrawString(32, 44, 1, 1, "Level 3", OLEDC_COLOR_WHITE);
    oledC_DrawString(32, 60, 1, 1, "Level 4", OLEDC_COLOR_WHITE);
    while (!((PORTA & (1 << 12)) == 0))
    {
        if ((PORTA & (1 << 11)) == 0)
        {
            levelChoice++;
            oledC_DrawString(32, 12, 1, 1, "Level 1", OLEDC_COLOR_BLACK);
            oledC_DrawString(32, 28, 1, 1, "Level 2", OLEDC_COLOR_BLACK);
            oledC_DrawString(32, 44, 1, 1, "Level 3", OLEDC_COLOR_BLACK);
            oledC_DrawString(32, 60, 1, 1, "Level 4", OLEDC_COLOR_BLACK);
            if (levelChoice == 5)
                levelChoice = 1;
            if (levelChoice == 1)
            {
                oledC_DrawString(32, 12, 1, 1, "Level 1", OLEDC_COLOR_RED);
                oledC_DrawString(32, 28, 1, 1, "Level 2", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 44, 1, 1, "Level 3", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 60, 1, 1, "Level 4", OLEDC_COLOR_WHITE);
            }
            else if (levelChoice == 2)
            {
                oledC_DrawString(32, 12, 1, 1, "Level 1", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 28, 1, 1, "Level 2", OLEDC_COLOR_RED);
                oledC_DrawString(32, 44, 1, 1, "Level 3", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 60, 1, 1, "Level 4", OLEDC_COLOR_WHITE);
            }
            else if (levelChoice == 3)
            {
                oledC_DrawString(32, 12, 1, 1, "Level 1", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 28, 1, 1, "Level 2", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 44, 1, 1, "Level 3", OLEDC_COLOR_RED);
                oledC_DrawString(32, 60, 1, 1, "Level 4", OLEDC_COLOR_WHITE);
            }
            else if (levelChoice == 4)
            {
                oledC_DrawString(32, 12, 1, 1, "Level 1", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 28, 1, 1, "Level 2", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 44, 1, 1, "Level 3", OLEDC_COLOR_WHITE);
                oledC_DrawString(32, 60, 1, 1, "Level 4", OLEDC_COLOR_RED);
            }
        }
    }
    oledC_clearScreen();
    return levelChoice;
}

Barrier createBarrier(int direction, int posX, int posY, int size)
{
    Barrier newBarrier;
    newBarrier.direction = direction;
    newBarrier.posX = posX;
    newBarrier.posY = posY;
    newBarrier.size = size;
    return newBarrier;
}

void buildLevel(Level *level, int levelNumber)
{
    // ============================================================================
    // MAZE 1 - 20 barriers
    // ============================================================================

    const int maze1_orientations[] = {
        0, 0, 0, 1, 1, 1, 0, 1, 0, 0,
        0, 0, 0, 1, 1, 0, 1, 0, 0, 0};

    const int maze1_xs[] = {
        11, 0, 11, 37, 50, 63, 63, 85, 76, 63,
        50, 0, 37, 24, 11, 24, 85, 37, 24, 37};

    const int maze1ys[] = {
        19, 40, 61, 19, 0, 19, 19, 0, 40, 61,
        82, 82, 103, 82, 103, 124, 145, 145, 166, 180};

    const int maze1_lengths[] = {
        26, 26, 26, 84, 82, 42, 11, 40, 9, 32,
        35, 24, 58, 98, 87, 61, 45, 48, 48, 48};

    // ============================================================================
    // MAZE 2 - 20 barriers
    // ============================================================================

    const int maze2_orientations[] = {
        0, 0, 0, 0, 0, 0, 1, 0, 1, 1,
        0, 1, 0, 1, 0, 1, 0, 1, 1, 0};

    const int maze2_xs[] = {
        0, 62, 9, 0, 40, 0, 84, 9, 73, 9,
        20, 62, 9, 51, 20, 40, 9, 29, 20, 20};

    const int maze2ys[] = {
        14, 14, 30, 46, 46, 62, 62, 78, 78, 78,
        94, 94, 110, 110, 126, 126, 142, 142, 158, 172};

    const int maze2_lengths[] = {
        53, 33, 77, 31, 55, 84, 110, 64, 76, 112,
        42, 76, 42, 46, 20, 46, 20, 14, 14, 64};

    // ============================================================================
    // MAZE 3 - 17 barriers
    // ============================================================================

    const int maze3_orientations[] = {
        1, 1, 0, 0, 0, 0, 1, 1, 1, 0,
        1, 0, 1, 0, 0, 0, 0};

    const int maze3_xs[] = {
        11, 24, 24, 24, 37, 24, 63, 80, 50, 0,
        37, 11, 24, 0, 11, 37, 50};

    const int maze3ys[] = {
        19, 19, 19, 40, 61, 82, 19, 0, 82, 103,
        103, 124, 124, 145, 166, 166, 145};

    const int maze3_lengths[] = {
        84, 63, 39, 26, 26, 26, 107, 126, 63, 37,
        63, 13, 66, 11, 13, 47, 45};

    // ============================================================================
    // MAZE 4 - 13 barriers
    // ============================================================================

    const int maze4_orientations[] = {
        1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
        1, 1, 1};

    const int maze4_xs[] = {
        10, 22, 70, 82, 34, 22, 34, 22, 34, 22,
        34, 46, 58};

    const int maze4ys[] = {
        19, 0, 19, 0, 19, 40, 61, 82, 103, 124,
        124, 145, 124};

    const int maze4_lengths[] = {
        171, 171, 171, 171, 36, 36, 36, 36, 36, 36,
        45, 45, 45};

    // ============================================================================
    // HELPER ARRAYS FOR EASY ACCESS
    // ============================================================================

    // Array of pointers to direction arrays
    const int *selected_maze_directions[] = {
        maze1_orientations,
        maze2_orientations,
        maze3_orientations,
        maze4_orientations};

    // Array of pointers to x-coordinate arrays
    const int *selected_maze_xs[] = {
        maze1_xs,
        maze2_xs,
        maze3_xs,
        maze4_xs};

    // Array of pointers to y-coordinate arrays
    const int *selected_maze_ys[] = {
        maze1ys,
        maze2ys,
        maze3ys,
        maze4ys};

    // Array of pointers to length arrays
    const int *selected_maze_lengths[] = {
        maze1_lengths,
        maze2_lengths,
        maze3_lengths,
        maze4_lengths};

    // Number of barriers in each selected maze
    const int selected_maze_barrier_counts[] = {
        20, 20, 17, 13};

    for (int i = 0; i < selected_maze_barrier_counts[levelNumber - 1]; i++)
    {
        level->barriers[i] = createBarrier(selected_maze_directions[levelNumber - 1][i],
                                           selected_maze_xs[levelNumber - 1][i],
                                           selected_maze_ys[levelNumber - 1][i],
                                           selected_maze_lengths[levelNumber - 1][i]);
    }

    level->barrierCount = selected_maze_barrier_counts[levelNumber - 1];
}

void renderBarrier(Barrier barrier, int cameraY)
{
    int startY = barrier.posY - cameraY;
    int endY = startY + barrier.size;
    if (barrier.direction == 0)
    { // If the barrier is horizontal
        if (!(startY < 0 || startY > 96))
        {
            oledC_DrawRectangle(barrier.posX, startY, barrier.posX + barrier.size, startY, OLEDC_COLOR_WHITE);
        }
    }
    else
    { // If the barrier is vertical
        if (endY < 0)
        {
            return;
        }
        if (startY < 0)
        {
            startY = 0;
        }
        oledC_DrawRectangle(barrier.posX, startY, barrier.posX, endY, OLEDC_COLOR_WHITE);
    }
}

void clearBarrier(Barrier barrier, int lastY)
{
    int startY = barrier.posY - lastY;
    int endY = startY + barrier.size;
    if (barrier.direction == 0)
    { // If the barrier is horizontal
        if (!(startY < 0))
        {
            oledC_DrawRectangle(barrier.posX, startY, barrier.posX + barrier.size, startY, OLEDC_COLOR_BLACK);
        }
    }
    else
    { // If the barrier is vertical
        if (endY < 0)
        {
            return;
        }
        if (startY < 0)
        {
            startY = 0;
        }
        oledC_DrawRectangle(barrier.posX, startY, barrier.posX, endY, OLEDC_COLOR_BLACK);
    }
}

int checkCollision(Player oldPlayer, int newX, int newY, Barrier barrier)
{
    if (barrier.direction == 0)
    { // If the barrier is horizontal
        return ((oldPlayer.posY <= barrier.posY && newY >= barrier.posY) || (oldPlayer.posY >= barrier.posY && newY <= barrier.posY)) &&
               ((newX >= barrier.posX && newX <= barrier.posX + barrier.size) || (oldPlayer.posX >= barrier.posX && oldPlayer.posX <= barrier.posX + barrier.size));
    }
    else
    { // If the barrier is vertical
        return ((oldPlayer.posX <= barrier.posX && newX >= barrier.posX) || (oldPlayer.posX >= barrier.posX && newX <= barrier.posX)) &&
               ((newY >= barrier.posY && newY <= barrier.posY + barrier.size) || (oldPlayer.posY >= barrier.posY && oldPlayer.posY <= barrier.posY + barrier.size));
    }
}

void inputPlayerName(char *playerName)
{
    int potValue = 0;
    playerName[6] = '\0';
    int charPosition = 0;
    char currentLetter = 'A';
    oledC_clearScreen();
    oledC_DrawString(2, 4, 1, 1, "Enter Name", OLEDC_COLOR_CYAN);

    oledC_DrawString(2, 20, 1, 1, "S1: Next", OLEDC_COLOR_CYAN);
    oledC_DrawString(2, 30, 1, 1, "S2: Prev", OLEDC_COLOR_CYAN);
    oledC_DrawString(2, 40, 1, 1, "Both: Save", OLEDC_COLOR_CYAN);

    while (1)
    {
        int btn1Pressed = detectButton1();
        int btn2Pressed = detectButton2();
        int bothPressed = (!detectButton1()) && (!detectButton2());

        if (bothPressed)
        {
            break; // Save name when both buttons pressed together
        }

        if (btn1Pressed)
        {
            charPosition++; // Move to next character
            if (charPosition > 11)
                charPosition = 0;

            DELAY_milliseconds(150); // Debounce delay
        }

        if (btn2Pressed)
        {
            charPosition--; // Move to previous character
            if (charPosition < 0)
                charPosition = 0;

            DELAY_milliseconds(150); // Debounce delay
        }

        // Clear current character display
        oledC_DrawCharacter(2 + charPosition * 5, 30, 1, 1, playerName[charPosition], OLEDC_COLOR_BLACK);

        // Get letter from potentiometer
        potValue = (int)(readPotentiometer() / (float)158);
        currentLetter = potValue + 'A';
        playerName[charPosition] = currentLetter;

        // Draw the character
        oledC_DrawCharacter(2 + charPosition * 5, 30, 1, 1, currentLetter, OLEDC_COLOR_WHITE);
        DELAY_milliseconds(150);
    }
}

int main(void)
{
    I2Cerror status; // for the ACC driver
    int accelX, accelY, potValue = 0;
    unsigned char accelData[6] = {0}; // array for XYZ from Acc
    SYSTEM_Initialize();
    status = AccelSetup(status);
    SystemSetup();
    DELAY_milliseconds(100);

    ClockSetup(); // start counting time for the game

    levelDuration = 6250; // makes sure the game wont end in the menu;
    Player player;        // initialize player
    Level currentLevel;   // initialize the Level
    Camera gameCamera;    // initialize the camera

    // Initialize top scores
    for (int i = 0; i < TOP_SCORES_COUNT; i++)
    {
        strcpy(topScores[i].playerName, "EMPTY");
        topScores[i].finalScore = 0;
        topScores[i].levelNumber = 1;
    }

    for (;;)
    {
        IEC0bits.T1IE = 0;
        oledC_clearScreen();
        char playerName[7] = "AAAAA\0";
        int difficultyChoice = 0;
        float speedFactor = 1;
        int horizontalCollision = 0;
        int verticalCollision = 0;
        int lastPotValue = -1; // set Old potentiometer to -1 to start the level when the game start
        int potDifference = 0; // initialize the potentiometer difference to 0
        int levelChoice = 0;

        difficultyChoice = selectDifficulty();
        levelChoice = selectLevel();
        buildLevel(&currentLevel, levelChoice); // put the barriers of a level in the Level

        // Set level times based on difficulty: {40, 30, 20}
        switch (difficultyChoice)
        {
        case 1:
            speedFactor = 0.8;
            levelDuration = 40;
            break;
        case 2:
            speedFactor = 1.0; // Normal speed
            levelDuration = 30;
            break;
        case 3:
            speedFactor = 1.2;
            levelDuration = 20;
            break;
        default:
            speedFactor = 1.0;
            levelDuration = 30;
            break;
        }

        player.posX = 92; // start the player in the right side of the screen
        player.posY = 4;  // start the player at the top of the screen

        gameCamera.currentY = 0; // start the camera at the top of the level
        gameCamera.height = 96;  // set the size of the camera to the size of the screen
        gameCamera.lastY = 0;    // initialize the LastY of the camera

        timeCounter = 0;
        gameRunning = 1;
        IEC0bits.T1IE = 1;
        for (;;)
        { // start of game loop
            if (gameRunning == 0)
            {
                break;
            } // when timer ends, exit game loop
            for (int i = 0; i < 4; i++)
            {
                status = i2cReadSlaveRegister(0x3A, 0x32 + i, &accelData[i]); // read the ACC into accelData[]

                DELAY_microseconds(50);
            }

            potValue = readPotentiometer();               // get the potentiometer position between 0 to 4095
            potValue = (potValue / 42.6);                 // make it from 4096 to 96
            potDifference = abs(potValue - lastPotValue); // checks of the potentiometer position changed
            if (potDifference)
            {
                IEC0bits.T1IE = 0; // turn off the interrupts so the counter wont smear
                for (int i = 0; i < currentLevel.barrierCount; i++)
                {
                    clearBarrier(currentLevel.barriers[i], gameCamera.currentY); // if the potentiometer position changed, remove the barriers
                }
                if (player.posY > gameCamera.currentY && player.posY < (gameCamera.currentY + gameCamera.height))
                    oledC_DrawCircle(player.posX, player.posY - gameCamera.currentY, 2, OLEDC_COLOR_BLACK); // if the player moved delete the old player
                gameCamera.currentY = potValue;                                                             // update the camera new position from the potentiometer
                for (int i = 0; i < currentLevel.barrierCount; i++)
                {
                    renderBarrier(currentLevel.barriers[i], gameCamera.currentY); // draw the barriers in the new location based of the new camera location
                }
                if (player.posY > gameCamera.currentY && player.posY < (gameCamera.currentY + gameCamera.height))
                    oledC_DrawCircle(player.posX, player.posY - gameCamera.currentY, 2, OLEDC_COLOR_CYAN);
                IEC0bits.T1IE = 1; // re-enable interrupts
            }

            if (player.posY < gameCamera.currentY || player.posY > (gameCamera.currentY + gameCamera.height))
                continue;

            accelX = -(accelData[0] + accelData[1] * 256); // gets the tilt on the x axis
            accelY = accelData[2] + accelData[3] * 256;    // gets the tilt on the y axis

            int newPlayerX = (int)(player.posX + ((float)accelX / 50) * speedFactor);
            int newPlayerY = (int)(player.posY + ((float)accelY / 50) * speedFactor);

            if (!((player.posX == newPlayerX) && (player.posY == newPlayerY)))
            {
                oledC_DrawCircle(player.posX, player.posY - gameCamera.currentY, 2, OLEDC_COLOR_BLACK); // if the player moved delete the old player

                horizontalCollision = 0;
                verticalCollision = 0;

                for (int i = 0; i < currentLevel.barrierCount; i++)
                {
                    if (checkCollision(player, newPlayerX + 2, player.posY, currentLevel.barriers[i]) || checkCollision(player, newPlayerX - 2, player.posY, currentLevel.barriers[i]))
                    { // checks for horizontal collisions with barriers
                        horizontalCollision = 1;
                    }
                    if (checkCollision(player, player.posX, newPlayerY + 2, currentLevel.barriers[i]) || checkCollision(player, player.posX, newPlayerY - 2, currentLevel.barriers[i]))
                    { // checks for vertical collisions with barriers
                        verticalCollision = 1;
                    }
                }

                if (!horizontalCollision)
                { // if there is no horizontal collision
                    player.posX = newPlayerX;
                }
                if (!verticalCollision)
                { // if there is no vertical collision
                    player.posY = newPlayerY;
                }

                if (player.posX < 6 && player.posY > 186)
                {
                    IEC0bits.T1IE = 0;
                    gameRunning = 0;

                    // Calculate percentage score
                    int maxTime = levelDuration * 10;
                    int usedTime = timeCounter;
                    playerPoints = ((maxTime - usedTime) * 100) / maxTime;
                    if (playerPoints < 0)
                        playerPoints = 0;

                    // Check if score qualifies for top 3
                    if (playerPoints > topScores[2].finalScore || topScores[2].finalScore == 0)
                    {
                        oledC_sendCommand(OLEDC_CMD_SET_DISPLAY_MODE_ON, NULL, 0);
                        displayInvert = 1;
                        inputPlayerName(playerName);
                        savePlayerScore(playerName, levelChoice);
                        showLeaderboard();
                        while (!detectButton1())
                        { // wait for S1 to be pressed to restart the game
                            DELAY_milliseconds(100);
                        }
                    }
                    break;
                }
                // make sure the player is in the boundaries of the level
                if (player.posX < 4)
                {
                    player.posX = 4;
                }
                if (player.posX > 92)
                {
                    player.posX = 92;
                }
                if (player.posY < 4)
                {
                    player.posY = 4;
                }
                if (player.posY > 188)
                {
                    player.posY = 188;
                }

                IEC0bits.T1IE = 0; // turn off the interrupts so the counter wont smear
                if (newPlayerY > gameCamera.currentY && newPlayerY < (gameCamera.currentY + gameCamera.height))
                {
                    oledC_DrawCircle(player.posX, player.posY - gameCamera.currentY, 2, OLEDC_COLOR_CYAN);
                }
                IEC0bits.T1IE = 1; // re-enable interrupts
            }
            lastPotValue = potValue;
            DELAY_milliseconds(10); // delay to control the game speed
        }
        IEC0bits.T1IE = 0;
        oledC_clearScreen();

        if (gameExited)
        {
            showGameOver();
            gameExited = 0;
        }
        else if (gameRunning == 0 && player.posX >= 6)
        {
            showGameOver();
        }

        while (PORTA & (1 << 11))
        {
        }
    }
}
/**
 End of File
*/
