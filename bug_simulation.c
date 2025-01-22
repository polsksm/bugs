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

void DisplayBugDNA(Bug *bug) {
  int i;

  for (i = 31; i >= 0; i--) {
    printf("%u", (bug->dna >> i) & 1);
    if (i % 8 == 0)
      printf(" ");
  }
  printf("\nHealth: (%u) -> %u\n", bug->health, bug->dna >> 24 & 0xff);
  printf("Sex: (%u) %u\n", bug->sex, bug->dna >> 23 & 0x01);
  printf("Vision:(%u) %u\n", bug->vision, bug->dna >> 21 & 0x03);
  printf("Speed:(%u) %u\n", bug->speed, bug->dna >> 19 & 0x03);
  printf("Drive:(%u) %u\n", bug->drive, bug->dna >> 15 & 0x0f);
}

Bug *ImmaculateBirthABug(int i, Bug *bugs) {
  bugs = realloc(bugs, (g_numBugs + 1) * sizeof(Bug));
  if (bugs == NULL) {
    perror("Error: Unable to allocate memory for bugs\n");
    exit(1);
  }
  bugs[g_numBugs].x = i % WORLD_WIDTH;
  bugs[g_numBugs].y = i / WORLD_WIDTH;
  bugs[g_numBugs].isAlive = 1;
  bugs[g_numBugs].sex = rand() % 2;
  bugs[g_numBugs].health = 255;
  bugs[g_numBugs].vision = rand() % 5;
  bugs[g_numBugs].speed = rand() % 5;
  bugs[g_numBugs].drive = rand() % 17;
  // dna is the health/sex/vision etc combined into one number
  bugs[g_numBugs].dna = bugs[g_numBugs].health << 24;
  bugs[g_numBugs].dna |= bugs[g_numBugs].sex << 23;
  bugs[g_numBugs].dna |= bugs[g_numBugs].vision << 21;
  bugs[g_numBugs].dna |= bugs[g_numBugs].speed << 19;
  bugs[g_numBugs].dna |= bugs[g_numBugs].drive << 15;

  // bugs[g_numBugs].dna |= bugs[g_numBugs].vision << 8 | bugs[g_numBugs].speed;
  // bugs[g_numBugs].dna |= bugs[g_numBugs].drive;

  DisplayBugDNA(&bugs[g_numBugs]);

  g_worldCell[i].color.r = bugs[g_numBugs].dna >> 24 & 0xff;
  g_worldCell[i].color.g = bugs[g_numBugs].dna >> 16 & 0xff;
  g_worldCell[i].color.b = bugs[g_numBugs].dna >> 8 & 0xff;
  g_worldCell[i].color.a = 255;
  g_worldCell[i].type = 3;
  ++g_numBugs;
  return bugs;
}

void recalculateDNA(Bug *bug) {
  bug->dna = bug->health << 24;
  bug->dna |= bug->sex << 23;
  bug->dna |= bug->vision << 21;
  bug->dna |= bug->speed << 19;
  bug->dna |= bug->drive << 15;
}
void UpdateStatusLine(Bug *bugs) {
  char status_line[100];
  u_int32_t alive = 0;
  for (int i = 0; i < g_numBugs; ++i) {
    if (bugs[i].isAlive)
      ++alive;
  }
  sprintf(status_line, "BUGS: %u", alive);
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
  SetTargetFPS(120);

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

  // Main game loop
  u_int64_t frame = 0;
  UpdateStatusLine(bugs);
  while (!WindowShouldClose()) {
    for (int i = 0; i < g_numBugs; ++i) {
      if (!bugs[i].isAlive)
        continue;
      int screen_pos = bugs[i].y * WORLD_WIDTH + bugs[i].x;
      g_worldCell[screen_pos].color = BLACK;
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
      screen_pos = bugs[i].y * WORLD_WIDTH + bugs[i].x;

      bugs[i].health -= 1;
      if (bugs[i].health == 0) {
        g_worldCell[screen_pos].color = BLACK;
        bugs[i].isAlive = 0;
      }

      if (g_worldCell[screen_pos].type == 1) {
        bugs[i].health += bugs[i].health > 245 ? 255 - bugs[i].health : 10;
      } else if (g_worldCell[screen_pos].type == 2) {
        bugs[i].health -= bugs[i].health < 10 ? bugs[i].health : 10;
        if (bugs[i].health == 0) {
          g_worldCell[screen_pos].color = BLACK;
          bugs[i].isAlive = 0;
        }
      }
      recalculateDNA(&bugs[i]);
      // if we decide to leave the carcass on the screen
      // this if switch needs to be removed
      if (bugs[i].isAlive) {
        // set screen pixel to bug color
        g_worldCell[screen_pos].color.r = bugs[i].dna >> 24 & 0xff;
        g_worldCell[screen_pos].color.g = bugs[i].dna >> 16 & 0xff;
        g_worldCell[screen_pos].color.b = bugs[i].dna >> 8 & 0xff;
        g_worldCell[screen_pos].color.a = 255;
      }
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
    UpdateStatusLine(bugs);
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
