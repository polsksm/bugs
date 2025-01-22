// compiled with: gcc
// -o bug_simulation bug_simulation.c -g -lraylib -lm -ldl -lpthread -lGL -lrt
#include "raylib.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WORLD_WIDTH 800
#define WORLD_HEIGHT 600
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 624

#define INIT_BUG_PROB 0.01
#define INIT_FOOD_PROB 0.01
#define INIT_POISON_PROB 0.005

typedef struct Bug {
  int x, y;             // Position
  int isAlive;          // 0|1
  unsigned char health; // 0 - 255
  unsigned char sex;    // 0|1
  unsigned char vision; // 0-4 distance the bug can see
  unsigned char speed;  // 0 - 4 - how many squares the bug can move
  unsigned char drive;  // 0 - 16 - how much the bug prefers sex over food
  uint32_t dna;         // this doubles for the bug's color
} Bug;

struct WorldCell {
  unsigned char type;
  Color color;
} *g_worldCell;
u_int64_t g_numBugs = 0;

Bug *ImmaculateBirthABug(int i, Bug *bugs) {
  bugs = realloc(bugs, (g_numBugs + 1) * sizeof(Bug));
  if (bugs == NULL) {
    perror("Error: Unable to allocate memory for bugs\n");
    exit(1);
  }
  bugs[g_numBugs].x = i % WORLD_WIDTH;
  bugs[g_numBugs].y = i / WORLD_WIDTH;
  g_worldCell[i].color = WHITE;
  g_worldCell[i].type = 3;
  ++g_numBugs;
  return bugs;
}

void UpdateStatusLine(void) {
  char status_line[100];
  sprintf(status_line, "BUGS: %lu", g_numBugs);
  DrawText(status_line, 10, SCREEN_HEIGHT - 23, 20, WHITE);
}

int main(void) {

  g_worldCell = (struct WorldCell *)malloc(WORLD_WIDTH * WORLD_HEIGHT *
                                           sizeof(struct WorldCell));
  if (g_worldCell == NULL) {
    printf("Error: Unable to allocate memory for g_worldCell\n");
    return 1;
  }
  // Initialize raylib
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Bug Simulation");
  SetTargetFPS(60);

  // Initialize random number generator
  srand(time(NULL));

  // Create bugs
  Bug *bugs = NULL;

  printf("Initializing world\n");
  for (int i = 0; i < WORLD_WIDTH * WORLD_HEIGHT; i++) {
    if (rand() % 100 < INIT_BUG_PROB * 100) {
      bugs = ImmaculateBirthABug(i, bugs);
    } else if (rand() % 100 < INIT_FOOD_PROB * 100) {
      g_worldCell[i].type = 1;
      g_worldCell[i].color = GREEN;
    } else if (rand() % 100 < INIT_POISON_PROB * 100) {
      g_worldCell[i].type = 2;
      g_worldCell[i].color = RED;
    } else {
      g_worldCell[i].type = 0;
      g_worldCell[i].color = BLACK;
    }
  }

  UpdateStatusLine();

  // Main game loop
  u_int64_t frame = 0;
  while (!WindowShouldClose()) {
    for (int i = 0; i < g_numBugs; ++i) {
      g_worldCell[bugs[i].y * WORLD_WIDTH + bugs[i].x].color = BLACK;
      // Move bugs randomly
      bugs[i].x += (rand() % 3) - 1;
      bugs[i].y += (rand() % 3) - 1;
      // Wrap around screen
      if (bugs[i].x < 0)
        bugs[i].x = WORLD_WIDTH - 1;
      if (bugs[i].x >= WORLD_WIDTH)
        bugs[i].x = 0;
      if (bugs[i].y < 0)
        bugs[i].y = WORLD_HEIGHT - 1;
      if (bugs[i].y >= WORLD_HEIGHT)
        bugs[i].y = 0;
      /*
      if (g_worldCell[bugs[i].y * WORLD_WIDTH + bugs[i].x].type == 1) {
        bugs[i].health += bugs[i].health > 245 ? 255 - bugs[i].health : 10;
      } else if (g_worldCell[bugs[i].y * WORLD_WIDTH + bugs[i].x].type == 2) {
        bugs[i].health -= bugs[i].health < 10 ? bugs[i].health : 10;
        if (bugs[i].health == 0) {
          g_worldCell[bugs[i].y * WORLD_WIDTH + bugs[i].x].color = BLACK;
          for (int j = i; j < g_numBugs - 1; j++) {
            bugs[j] = bugs[j + 1];
          }
          --g_numBugs;
        }
      }
      */
      g_worldCell[bugs[i].y * WORLD_WIDTH + bugs[i].x].color = WHITE;
    }
    // Update bugs
    /*
    for (int i = 0; i < INIT_BUG_COUNT; i++) {
      // Move bugs randomly
      bugs[i].x += (rand() % 3) - 1;
      bugs[i].y += (rand() % 3) - 1;

      // Wrap around screen
      if (bugs[i].x < 0)
        bugs[i].x = WORLD_WIDTH - 1;
      if (bugs[i].x >= WORLD_WIDTH)
        bugs[i].x = 0;
      if (bugs[i].y < 0)
        bugs[i].y = WORLD_HEIGHT - 1;
      if (bugs[i].y >= WORLD_HEIGHT)
        bugs[i].y = 0;

      // Change color randomly
      if (rand() % 100 < 5) {
        bugs[i].color = (Color){rand() % 256, rand() % 256, rand() % 256, 255};
      }
  }
      */

    // Draw frame
    BeginDrawing();
    ClearBackground(BLACK);
    UpdateStatusLine();
    for (int i = 0; i < WORLD_HEIGHT * WORLD_WIDTH; ++i) {
      // DrawPixel(g_worldCell[i]., bugs[i].y, bugs[i].color);
      DrawPixel(i % WORLD_WIDTH, i / WORLD_WIDTH, g_worldCell[i].color);
    }
    EndDrawing();
    ++frame;
    if (frame % 100 == 0)
      printf("Frame: %lu\n", frame);
  }

  // Deinitialize raylib
  CloseWindow();

  return 0;
}
