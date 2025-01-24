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
#define INIT_FOOD_PROB 0.9
#define INIT_POISON_PROB 0.00

Color FOOD_COLOR = {255, 255, 0, 255};

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
typedef enum { EMPTY = 0, FOOD = 1, POISON = 2, BUG = 3 } WORLDCELL_TYPE;
struct WorldCell {
  WORLDCELL_TYPE type[WORLD_WIDTH * WORLD_HEIGHT];
  Color color[WORLD_WIDTH * WORLD_HEIGHT];
  Bug *bug[WORLD_WIDTH * WORLD_HEIGHT];
} *g_worldCell;

u_int64_t g_numBugs = 0;

void bugDeath(Bug *bugs, u_int64_t screen_pos);
void DisplayBugDNA(Bug *bug);
Bug *ImmaculateBirthABug(int i, Bug *bugs);
void recalculateDNA(Bug *bug);
void UpdateStatusLine(Bug *bugs, u_int64_t frame, FILE *ofp);
u_int64_t moveBug(Bug *bug);
int bugsAreSameSex(Bug *bug1, Bug *bug2);

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
  printf("Bug %lu: x: %d y: %d\n", g_numBugs, bugs[g_numBugs].x,
         bugs[g_numBugs].y);
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

  // DisplayBugDNA(&bugs[g_numBugs]);

  g_worldCell->color[i].r = bugs[g_numBugs].dna >> 24 & 0xff;
  g_worldCell->color[i].g = bugs[g_numBugs].dna >> 16 & 0xff;
  g_worldCell->color[i].b = bugs[g_numBugs].dna >> 8 & 0xff;
  g_worldCell->color[i].a = 255;
  g_worldCell->type[i] = BUG;
  g_worldCell->bug[i] = &bugs[g_numBugs];
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
void UpdateStatusLine(Bug *bugs, u_int64_t frame, FILE *ofp) {
  char status_line[100];
  u_int32_t alive = 0;
  u_int32_t health = 0;
  for (int i = 0; i < g_numBugs; ++i) {
    if (bugs[i].isAlive) {
      ++alive;
      health += bugs[i].health;
    }
  }
  sprintf(status_line, "BUGS: %u\tFrame: %lu", alive, frame);
  DrawText(status_line, 10, SCREEN_HEIGHT - 23, 20, WHITE);
  if (ofp)
    fprintf(ofp, "%u\t%lu\n", alive, frame);
}

u_int64_t moveBug(Bug *bug) {
  // Move bugs randomly
  bug->x += (rand() % 3) - 1;
  bug->y += (rand() % 3) - 1;
  // Wrap around screen
  if (bug->x < 0)
    bug->x = WORLD_WIDTH - 1;
  if (bug->x >= WORLD_WIDTH)
    bug->x = 0;
  if (bug->y < 0)
    bug->y = WORLD_HEIGHT - 1;
  if (bug->y >= WORLD_HEIGHT)
    bug->y = 0;
  bug->health -= 1;
  u_int64_t screen_pos = bug->y * WORLD_WIDTH + bug->x;
  if (bug->health == 0) {
    bugDeath(bug, screen_pos);
    // leave the carcass as food
    g_worldCell->color[screen_pos] = FOOD_COLOR;
    bug->isAlive = 0;
    g_worldCell->bug[screen_pos] = NULL;
    g_worldCell->type[screen_pos] = FOOD;
  }
  return screen_pos;
}
void bugDeath(Bug *bugs, u_int64_t screen_pos) {
  g_worldCell->color[screen_pos] = FOOD_COLOR;
  bugs->isAlive = 0;
  g_worldCell->bug[screen_pos] = NULL;
  g_worldCell->type[screen_pos] = FOOD;
}

int bugsAreSameSex(Bug *bug1, Bug *bug2) {
  return bug1->sex == bug2->sex ? 1 : 0;
}

/// @brief Create a new bug using the DNA of the parents
int birthABug(Bug *bug, u_int64_t screen_pos) {
  if (g_numBugs >= WORLD_WIDTH * WORLD_HEIGHT) {
    printf("Error: Too many bugs\n");
    return 0;
  }
  bug = realloc(bug, (g_numBugs + 1) * sizeof(Bug));
  if (bug == NULL) {
    perror("Error: Unable to allocate memory for bugs\n");
    exit(1);
  }

  Bug *baby = &bug[g_numBugs];
  Bug *mom = g_worldCell->bug[screen_pos];
  Bug *dad = bug;
  // DNA is a combination of the parents' DNA
  baby->health = (mom->health + dad->health) / 2;
  baby->vision = (mom->vision + dad->vision) / 2;
  baby->speed = (mom->speed + dad->speed) / 2;
  baby->drive = (mom->drive + dad->drive) / 2;
  baby->sex = rand() % 2;
  // perform  a mutation
  // 1 in 100 chance of a mutation
  if (rand() % 100 == 0) {
    int bit = rand() % 32;
    baby->dna ^= 1 << bit;
  }

  // find an empty spot close to the parents
  int x = screen_pos % WORLD_WIDTH;
  int y = screen_pos / WORLD_WIDTH;
  int dx = rand() % 3 - 1;
  int dy = rand() % 3 - 1;
  while (g_worldCell->type[(y + dy) * WORLD_WIDTH + (x + dx)] != EMPTY) {
    dx = rand() % 3 - 1;
    dy = rand() % 3 - 1;
  }
  baby->x = x + dx;
  baby->y = y + dy;
  if (baby->x < 0)
    baby->x = WORLD_WIDTH - 1;
  if (baby->x >= WORLD_WIDTH)
    baby->x = 0;
  if (baby->y < 0)
    baby->y = WORLD_HEIGHT - 1;
  if (baby->y >= WORLD_HEIGHT)
    baby->y = 0;
  baby->isAlive = 1;
  recalculateDNA(baby);

  return 1;
}

int main(int argc, char **argv) {
  FILE *ofp = NULL;
  if (argc == 1) {
    ofp = fopen("bug_simulation.log", "w");
  }

  g_worldCell = (struct WorldCell *)malloc(sizeof(struct WorldCell));

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
  for (int i = 0; i < WORLD_WIDTH * WORLD_HEIGHT; ++i) {
    if (rand() % 100 < INIT_BUG_PROB * 100) {
      bugs = ImmaculateBirthABug(i, bugs);
    } else if (rand() % 100 < INIT_FOOD_PROB * 100) {
      g_worldCell->type[i] = FOOD;
      g_worldCell->color[i] = FOOD_COLOR;
      g_worldCell->color[i].a = 128;
    } else if (rand() % 100 < INIT_POISON_PROB * 100) {
      g_worldCell->type[i] = POISON;
      g_worldCell->color[i] = RED;
    } else {
      g_worldCell->type[i] = EMPTY;
      g_worldCell->color[i] = BLACK;
    }
  }

  u_int64_t frame = 0;
  UpdateStatusLine(&bugs[0], frame, ofp);

  Image img = {.data = g_worldCell->color,
               .width = WORLD_WIDTH,
               .height = WORLD_HEIGHT,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D texture = LoadTextureFromImage(img);

  // Main game loop
  while (!WindowShouldClose()) {
    for (int i = 0; i < g_numBugs; ++i) {
      if (!bugs[i].isAlive)
        continue;
      u_int64_t screen_pos = bugs[i].y * WORLD_WIDTH + bugs[i].x;
      g_worldCell->color[screen_pos] = BLACK;
      g_worldCell->bug[screen_pos] = NULL;
      g_worldCell->type[screen_pos] = EMPTY;

      screen_pos = moveBug(&bugs[i]);
      // moving could mean the death of the bug
      if (!bugs[i].isAlive)
        continue;

      if (g_worldCell->type[screen_pos] == FOOD) {
        bugs[i].health += bugs[i].health > 245 ? 255 - bugs[i].health : 10;
      } else if (g_worldCell->type[screen_pos] == POISON) {
        bugs[i].health -= bugs[i].health < 10 ? bugs[i].health : 10;
        if (bugs[i].health == 0) {
          bugDeath(&bugs[i], screen_pos);
          continue;
        }
      } else if (g_worldCell->type[screen_pos] == BUG) {
        if (bugsAreSameSex(g_worldCell->bug[screen_pos], &bugs[i])) {
          birthABug(&bugs[i], screen_pos);
        }
      }
      recalculateDNA(&bugs[i]);
      if (bugs[i].isAlive) {
        // set screen pixel to bug color
        g_worldCell->color[screen_pos].r = bugs[i].dna >> 24 & 0xff;
        g_worldCell->color[screen_pos].g = bugs[i].dna >> 16 & 0xff;
        g_worldCell->color[screen_pos].b = bugs[i].dna >> 8 & 0xff;
        g_worldCell->color[screen_pos].a = 255;
        g_worldCell->type[screen_pos] = BUG;
        g_worldCell->bug[screen_pos] = &bugs[i];
      }
    }

    // Draw frame
    BeginDrawing();
    ClearBackground(BLACK);
    UpdateStatusLine(bugs, frame, ofp);
    UpdateTexture(texture, g_worldCell->color);
    DrawTexture(texture, 0, 0, WHITE);
    /*
    for (int i = 0; i < WORLD_HEIGHT * WORLD_WIDTH; ++i) {
      DrawPixel(i % WORLD_WIDTH, i / WORLD_WIDTH, g_worldCell[i].color);
    }
    */
    EndDrawing();
    ++frame;
    if (frame % 100 == 0)
      printf("Frame: %lu\n", frame);
  }

  // Deinitialize raylib
  CloseWindow();
  free(g_worldCell);
  free(bugs);
  if (ofp)
    fclose(ofp);

  return 0;
}
