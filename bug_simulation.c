// compiled with: gcc
// -o bug_simulation bug_simulation.c -g -lraylib -lm -ldl -lpthread -lGL -lrt
#include "raylib.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define WORLD_WIDTH 800
#define WORLD_HEIGHT 600
#define SCREEN_WIDTH 1820
#define SCREEN_HEIGHT 980

#define INIT_BUG_PROB 0.001
#define INIT_FOOD_PROB 0.2
#define INIT_POISON_PROB 0.01

#define REGENERATE_FOOD_RATE 0.0026
#define MUTATION_RATE 0.20

#define FOOD_HEALTH 10
#define MOVE_COST_PROB 1.0
#define POISON_COST 10
#define MOVE_COST 1
#define MATING_COST 10
#define MIN_MATING_AGE 10
#define MIN_FIGHTING_AGE 15
#define FIGHTING_COST 100
int PAUSE = 0;

Color FOOD_COLOR = {255, 255, 0, 255};
#define FOOD_OPACITY 100

typedef struct Bug {
  int x, y;             // Position
  int isAlive;          // 0|1
  int age;              // how many frames they've been alive for
  unsigned char health; // 0 - 255
  unsigned char sex;    // 0|1
  unsigned char vision; // 0-4 distance the bug can see
  unsigned char speed;  // 0 - 4 - how many squares the bug can move
  unsigned char drive;  // 0 - 16 - how much the bug wnats to mate
  unsigned char aggr;   // 0 - 16 - how much the bug wants to fight
  uint32_t dna;         // this doubles for the bug's color
} Bug;

typedef enum { EMPTY = 0, FOOD = 1, POISON = 2, BUG = 3 } WORLDCELL_TYPE;

struct WorldCell {
  WORLDCELL_TYPE type[WORLD_WIDTH * WORLD_HEIGHT];
  Color color[WORLD_WIDTH * WORLD_HEIGHT];
  int bug_idx[WORLD_WIDTH * WORLD_HEIGHT];
} *g_worldCell;

u_int64_t g_numBugs = 0;
u_int32_t g_births = 0, g_fights = 0;
u_int32_t g_deaths = 0;

void bugDeath(Bug *bugs, int idx, u_int64_t screen_pos);
void displayBugDNA(Bug *bug);
Bug *immaculateBirthABug(int i, Bug *bugs);
void calculateDNA(Bug *bug);
void updateStatusLine(Bug *bugs, u_int64_t frame, FILE *ofp);
u_int64_t moveBug(Bug *bug);
int bugsAreSameSex(Bug *bug1, Bug *bug2);
// int birthABug(Bug *bug, u_int64_t screen_pos);

void displayBugDNA(Bug *bug) {
  int i;

  for (i = 31; i >= 0; i--) {
    printf("%u", (bug->dna >> i) & 1);
    if (i % 8 == 0)
      printf(" ");
  }
  printf("\nHealth: (%u) -> %u\n", bug->health, bug->dna & 0xff);
  printf("Sex: (%u) %u\n", bug->sex, bug->dna >> 31 & 0x01);
  printf("Vision:(%u) %u\n", bug->vision, bug->dna >> 29 & 0x03);
  printf("Speed:(%u) %u\n", bug->speed, bug->dna >> 27 & 0x03);
  printf("Drive:(%u) %u\n", bug->drive, bug->dna >> 23 & 0x0f);
  printf("Aggr:(%u) %u\n", bug->aggr, bug->dna >> 19 & 0x0f);
}

Bug *immaculateBirthABug(int i, Bug *bugs) {
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
  bugs[g_numBugs].age = 0;
  bugs[g_numBugs].sex = rand() % 2;
  bugs[g_numBugs].health = 255;
  bugs[g_numBugs].vision = rand() % 5;
  bugs[g_numBugs].speed = rand() % 5;
  bugs[g_numBugs].drive = rand() % 17;
  bugs[g_numBugs].aggr = rand() % 17;
  calculateDNA(&bugs[g_numBugs]);
  displayBugDNA(&bugs[g_numBugs]);
  g_worldCell->color[i].r = bugs[g_numBugs].dna >> 24 & 0xff;
  g_worldCell->color[i].g = bugs[g_numBugs].dna >> 16 & 0xff;
  g_worldCell->color[i].b = bugs[g_numBugs].dna >> 8 & 0xff;
  // health of the bug is also the alpha channel
  g_worldCell->color[i].a = bugs[g_numBugs].dna & 0xff;
  g_worldCell->type[i] = BUG;
  g_worldCell->bug_idx[i] = g_numBugs;
  ++g_numBugs;
  return bugs;
}

void calculateDNA(Bug *bug) {
  u_int16_t unused = 0xff;
  bug->dna = 0;
  bug->dna |= bug->sex << 31;
  bug->dna |= bug->vision << 29;
  bug->dna |= bug->speed << 27;
  bug->dna |= bug->drive << 23;
  bug->dna |= bug->aggr << 19;
  bug->dna |= unused << 11;
  bug->dna |= bug->health;
}
void updateStatusLine(Bug *bugs, u_int64_t frame, FILE *ofp) {
  char status_line[100];
  u_int32_t alive = 0;
  u_int32_t health = 0;
  u_int32_t drive = 0;
  u_int32_t aggr = 0;

  for (int i = 0; i < g_numBugs; ++i) {
    if (bugs[i].isAlive) {
      ++alive;
      health += bugs[i].health;
      drive += bugs[i].drive;
      aggr += bugs[i].aggr;
    }
  }
  sprintf(status_line, "BUGS: %6u\tFrame: %8lu", alive, frame);
  DrawText(status_line, 10, SCREEN_HEIGHT - 23, 20, WHITE);
  if (ofp)
    fprintf(ofp, "%u\t%lu\t%d\t%d\t%d\n", alive, frame, health / alive,
            drive / alive, aggr / alive);
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

  if (rand() % 100 < MOVE_COST_PROB * 100) {
    if (bug->health <= MOVE_COST) {
      bug->health = 0;
    } else
      bug->health -= MOVE_COST;
  }
  u_int64_t screen_pos = bug->y * WORLD_WIDTH + bug->x;
  return screen_pos;
}
void bugDeath(Bug *bugs, int idx, u_int64_t screen_pos) {
  if (bugs[idx].isAlive == 0) {
    return;
  }
  g_worldCell->color[screen_pos] = BLACK;
  bugs[idx].isAlive = 0;
  bugs[idx].age = 0;
  g_worldCell->bug_idx[screen_pos] = -1;
  g_worldCell->type[screen_pos] = EMPTY;
  ++g_deaths;
}

int bugsAreSameSex(Bug *bug1, Bug *bug2) {
  return bug1->sex == bug2->sex ? 1 : 0;
}

int getDeadBugIndex(Bug *bugs) {
  // printf("looking for dead bug among %lu bugs\n", g_numBugs);
  for (int i = 0; i < g_numBugs; ++i) {
    if (!bugs[i].isAlive) {
      // printf("Found dead bug at index %d\n", i);
      return i;
    }
  }
  // printf("Error: No dead bugs found\n");
  return -1;
}

/// @brief Create a new bug using the DNA of the parents
Bug *birthABug(Bug *bugs, int dad_idx, int mom_idx, u_int64_t mom_pos,
               u_int64_t dad_pos) {
  int baby_idx = getDeadBugIndex(bugs);

  if (baby_idx == -1 && g_numBugs >= WORLD_WIDTH * WORLD_HEIGHT) {
    printf("Error: Too many bugs\n");
    return bugs;
  }
  if (baby_idx == -1) {
    bugs = realloc(bugs, (g_numBugs + 1) * sizeof(Bug));
    if (bugs == NULL) {
      perror("Error: Unable to allocate memory for bugs\n");
      exit(1);
    }
    baby_idx = g_numBugs;
    ++g_numBugs;
  }

  Bug *baby = &bugs[baby_idx];
  Bug *mom = &bugs[mom_idx];
  Bug *dad = &bugs[dad_idx];
  // DNA is a combination of the parents' DNA
  baby->health = (mom->health + dad->health) / 2;
  baby->vision = (mom->vision + dad->vision) / 2;
  baby->speed = (mom->speed + dad->speed) / 2;
  baby->drive = (mom->drive + dad->drive) / 2;
  baby->aggr = (mom->aggr + dad->aggr) / 2;
  baby->sex = rand() % 2;
  baby->isAlive = 1;
  baby->age = 0;
  // perform  a mutation
  if (rand() % 100 >= MUTATION_RATE * 100) {
    int bit = rand() % 32;
    baby->dna ^= 1 << bit;
  }

  // find a place to put the baby
  for (int i = mom_pos - 1; i > 0; --i) {
    if (g_worldCell->type[i] == EMPTY) {
      baby->x = i % WORLD_WIDTH;
      baby->y = i / WORLD_WIDTH;
      break;
    }
  }
  if (baby->x == 0 && baby->y == 0) {
    for (int i = mom_pos + 1; i < WORLD_WIDTH * WORLD_HEIGHT; ++i) {
      if (g_worldCell->type[i] == EMPTY) {
        baby->x = i % WORLD_WIDTH;
        baby->y = i / WORLD_WIDTH;
        break;
      }
    }
  }
  if (baby->x == 0 && baby->y == 0) {
    printf("Error: No space to put baby\n");
    bugs[baby_idx].isAlive = 0;
  }

  calculateDNA(baby);

  if (mom->health <= MATING_COST) {
    mom->health = 0;
  } else
    mom->health -= MATING_COST;
  if (dad->health <= MATING_COST) {
    dad->health = 0;
  } else
    dad->health -= MATING_COST;
  ++g_births;

  return bugs;
}

//-------------------------------------------------------------
// bugFight - fight to the death
// ** updated - bug1 always wins - it can see bug2's health before the fight
// ----------------------------------------------------------
void bugFight(Bug *bugs, int idx1, int idx2, u_int64_t screen_pos) {

  int new_health = (bugs[idx1].health + bugs[idx2].health) - FIGHTING_COST;
  /*
  printf("bug2[%d](%d,%d) h:%d age:%d\tbug1[%d](%d,%d) h:%d age:%d\n", idx2,
         bugs[idx2].x, bugs[idx2].y, bugs[idx2].health, bugs[idx2].age, idx1,
         bugs[idx1].x, bugs[idx1].y, bugs[idx1].health, bugs[idx1].age);
         */

  bugs[idx2].health = 0;
  if (new_health > 255)
    new_health = 255;
  else if (new_health <= 0)
    new_health = 0;
  bugs[idx1].health = new_health;
  ++g_fights;
}

Bug *initializeWorld(Bug *bugs) {
  printf("Initializing world\n");
  for (int i = 0; i < WORLD_WIDTH * WORLD_HEIGHT; ++i) {
    if (rand() % 1000 < INIT_BUG_PROB * 1000) {
      bugs = immaculateBirthABug(i, bugs);
    } else if (rand() % 100 < INIT_FOOD_PROB * 100) {
      g_worldCell->type[i] = FOOD;
      g_worldCell->color[i] = FOOD_COLOR;
      g_worldCell->color[i].a = FOOD_OPACITY;
    } else if (rand() % 100 < INIT_POISON_PROB * 100) {
      g_worldCell->type[i] = POISON;
      g_worldCell->color[i] = RED;
    } else {
      g_worldCell->type[i] = EMPTY;
      g_worldCell->color[i] = BLACK;
    }
  }

  return bugs;
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
  for (int i = 0; i < WORLD_WIDTH * WORLD_HEIGHT; ++i) {
    g_worldCell->type[i] = EMPTY;
    g_worldCell->color[i] = BLACK;
    g_worldCell->bug_idx[i] = -1;
  }
  // Initialize raylib
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Bug Simulation");
  SetTargetFPS(120);

  // Initialize random number generator
  srand(time(NULL));

  // Create bugs
  Bug *bugs = NULL;

  bugs = initializeWorld(bugs);

  u_int64_t frame = 0;
  updateStatusLine(&bugs[0], frame, ofp);

  Image img = {.data = g_worldCell->color,
               .width = WORLD_WIDTH,
               .height = WORLD_HEIGHT,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D texture = LoadTextureFromImage(img);

  // set STDIN to non-blocking
  // Main game loop
  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_SPACE)) {
      PAUSE = !PAUSE;
      printf("PAUSE: %d\n", PAUSE);
    }
    if (!PAUSE) {
      for (int i = 0; i < g_numBugs; ++i) {
        if (!bugs[i].isAlive)
          continue;

        u_int64_t screen_pos = bugs[i].y * WORLD_WIDTH + bugs[i].x;
        u_int64_t old_screen_pos = screen_pos;
        g_worldCell->color[screen_pos] = BLACK;
        g_worldCell->bug_idx[screen_pos] = -1;
        g_worldCell->type[screen_pos] = EMPTY;
        screen_pos = moveBug(&bugs[i]);
        // moving could mean the death of the bug
        if (bugs[i].health == 0) {
          bugDeath(bugs, i, screen_pos);
          continue;
        }
        // the bug is in a new position (but not yet stored in the world)
        // see what they landed on and act accordingly
        if (g_worldCell->type[screen_pos] == FOOD) {
          bugs[i].health += bugs[i].health > 255 - FOOD_HEALTH
                                ? 255 - bugs[i].health
                                : FOOD_HEALTH;
        } else if (g_worldCell->type[screen_pos] == POISON) {
          bugs[i].health -=
              bugs[i].health < POISON_COST ? bugs[i].health : POISON_COST;
          if (bugs[i].health == 0) {
            bugDeath(bugs, i, screen_pos);
            continue;
          }
        } else if (g_worldCell->type[screen_pos] == BUG) {
          if (bugsAreSameSex(&bugs[g_worldCell->bug_idx[screen_pos]],
                             &bugs[i])) {
            if (bugs[i].drive > rand() % 16 && bugs[i].health > MATING_COST &&
                bugs[g_worldCell->bug_idx[screen_pos]].health > MATING_COST &&
                bugs[i].age >= MIN_MATING_AGE &&
                bugs[g_worldCell->bug_idx[screen_pos]].age >= MIN_MATING_AGE) {
              bugs = birthABug(bugs, i, g_worldCell->bug_idx[screen_pos],
                               screen_pos, old_screen_pos);
            }
          } else if (bugs[i].aggr > rand() % 16 &&
                     bugs[i].age >= MIN_FIGHTING_AGE &&
                     bugs[i].health >
                         bugs[g_worldCell->bug_idx[screen_pos]].health) {
            bugFight(bugs, i, g_worldCell->bug_idx[screen_pos], screen_pos);
            if (bugs[g_worldCell->bug_idx[screen_pos]].health == 0) {
              bugDeath(bugs, g_worldCell->bug_idx[screen_pos], screen_pos);
            }
            if (bugs[i].health == 0) {
              bugDeath(bugs, i, screen_pos);
              continue;
            }
          }
        }
        calculateDNA(&bugs[i]);
        if (bugs[i].isAlive) {
          // set screen pixel to bug color
          g_worldCell->color[screen_pos].r = bugs[i].dna >> 24 & 0xff;
          g_worldCell->color[screen_pos].g = bugs[i].dna >> 16 & 0xff;
          g_worldCell->color[screen_pos].b = bugs[i].dna >> 8 & 0xff;
          g_worldCell->color[screen_pos].a = bugs[i].dna & 0xff;
          g_worldCell->type[screen_pos] = BUG;
          g_worldCell->bug_idx[screen_pos] = i;
          bugs[i].age++;
        }
      }
      if (frame % 100 == 0) {
        printf("Frame: %lu Fights:%d Births:%d Deaths:%d  PopX:%d\n", frame,
               g_fights, g_births, g_deaths, g_births - g_deaths);
        g_births = 0;
        g_fights = 0;
        g_deaths = 0;
      }
      // regenerate food
      for (int i = 0; i < WORLD_WIDTH * WORLD_HEIGHT; ++i) {
        if (g_worldCell->type[i] == EMPTY &&
            rand() % 10000 < REGENERATE_FOOD_RATE * 10000) {
          g_worldCell->type[i] = FOOD;
          g_worldCell->color[i] = FOOD_COLOR;
          g_worldCell->color[i].a = FOOD_OPACITY;
        }
      }
      updateStatusLine(bugs, frame, ofp);
      ++frame;
    }
    // Draw frame
    BeginDrawing();
    ClearBackground(BLACK);
    UpdateTexture(texture, g_worldCell->color);
    DrawTexture(texture, 0, 0, WHITE);
    EndDrawing();
  }

  // Deinitialize raylib
  CloseWindow();
  free(g_worldCell);
  free(bugs);
  if (ofp)
    fclose(ofp);

  return 0;
}
