// compiled with: gcc
// -o bug_simulation bug_simulation.c -g
// -lraylib -lm -ldl -lpthread -lGL -lrt -lgsl
// opengl, raylib, gsl libraries required
#include "raylib.h"
#include <assert.h>
#include <fcntl.h>
#include <gsl/gsl_rng.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define WORLD_WIDTH 800
#define WORLD_HEIGHT 600
#define SCREEN_WIDTH 1820
#define SCREEN_HEIGHT 980

#define INIT_BUG_PROB 0.0011
#define INIT_FOOD_PROB 0.5
#define INIT_POISON_PROB 0.001

#define REGENERATE_FOOD_RATE 0.0040
#define REGENERATE_POISON_RATE 0.0001
#define MUTATION_RATE 0.30

#define FOOD_HEALTH 10
#define MOVE_COST_PROB 1.0
#define POISON_COST 100
#define MOVE_COST 1
#define MATING_COST 10
#define MIN_MATING_AGE 10
#define MIN_FIGHTING_AGE 15
#define FIGHTING_COST 50
int PAUSE = 0;

Color FOOD_COLOR = {0, 255, 0, 255};
#define FOOD_OPACITY 0

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

gsl_rng *g_rng;

void bugDeath(Bug *bugs, int idx, u_int64_t screen_pos);
void displayBugDNA(Bug *bug);
Bug *immaculateBirthABug(int i, Bug *bugs);
void calculateDNA(Bug *bug);
void updateStatusLine(Bug *bugs, u_int64_t frame, FILE *ofp);
u_int64_t bugMove(Bug *bug);
int bugsAreSameSex(Bug *bug1, Bug *bug2);
void getMovementProbabilities(Bug *bug, int *left_prob, int *up_prob);
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
  bugs[g_numBugs].sex = gsl_rng_get(g_rng) % 2;
  bugs[g_numBugs].health = 255;
  bugs[g_numBugs].vision = gsl_rng_get(g_rng) % 5;
  bugs[g_numBugs].speed = gsl_rng_get(g_rng) % 5;
  bugs[g_numBugs].drive = gsl_rng_get(g_rng) % 17;
  bugs[g_numBugs].aggr = gsl_rng_get(g_rng) % 17;
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

u_int64_t bugMove(Bug *bug) {
  // look around based on vision
  // the resources left, right, up and down from the bug will determine the
  // probability of moving in that direction
  //
  // Move bugs randomly if they have 0 vision
  if (bug->vision == 0) {
    bug->x += (gsl_rng_get(g_rng) % 3) - 1;
    bug->y += (gsl_rng_get(g_rng) % 3) - 1;
  } else {
    // if the bug has 1  or greater vision it can see the resources around it
    int left_prob = 0, up_prob = 0;
    getMovementProbabilities(bug, &left_prob, &up_prob);
    int x_dir = 0;
    if (gsl_rng_get(g_rng) % 100 < left_prob) {
      x_dir = -1;
    } else {
      x_dir = 1;
    }
    int y_dir = 0;
    if (gsl_rng_get(g_rng) % 100 < up_prob) {
      y_dir = -1;
    } else {
      y_dir = 1;
    }
    bug->x += x_dir;
    bug->y += y_dir;
  }

  // Wrap around screen
  if (bug->x < 0)
    bug->x = WORLD_WIDTH - 1;
  if (bug->x >= WORLD_WIDTH)
    bug->x = 0;
  if (bug->y < 0)
    bug->y = WORLD_HEIGHT - 1;
  if (bug->y >= WORLD_HEIGHT)
    bug->y = 0;

  if (gsl_rng_get(g_rng) % 100 < MOVE_COST_PROB * 100) {
    if (bug->health <= MOVE_COST) {
      bug->health = 0;
    } else
      bug->health -= MOVE_COST;
  }
  u_int64_t screen_pos = bug->y * WORLD_WIDTH + bug->x;
  assert(screen_pos < WORLD_WIDTH * WORLD_HEIGHT);
  assert(screen_pos >= 0);
  return screen_pos;
}

// sum up the resources to the left, right, up and down from the bug
// by summing up values of each resource type
//
void getMovementProbabilities(Bug *bug, int *left_prob, int *up_prob) {
  int screenx = 0, screeny = 0;
  double left = 0, right = 0, up = 0, down = 0;

  for (int startx = bug->x - bug->vision; startx <= bug->x + bug->vision;
       ++startx) {
    for (int starty = bug->y - bug->vision; starty <= bug->y + bug->vision;
         ++starty) {
      screenx = startx;
      screeny = starty;

      // Wrap-around calculations:
      if (screenx < 0)
        screenx = WORLD_WIDTH - (-screenx);
      if (screenx >= WORLD_WIDTH)
        screenx = screenx - WORLD_WIDTH;
      if (screeny < 0)
        screeny = WORLD_HEIGHT - (-screeny);
      if (screeny >= WORLD_HEIGHT)
        screeny = screeny - WORLD_HEIGHT;

      int screen_pos = screeny * WORLD_WIDTH + screenx;

      // Horizontal contributions
      if (startx < bug->x) {
        if (g_worldCell->type[screen_pos] == FOOD) {
          left += FOOD_HEALTH;
        } else if (g_worldCell->type[screen_pos] == POISON) {
          left -= POISON_COST;
        } else if (g_worldCell->type[screen_pos] == BUG) {
          left += bug->aggr + bug->drive;
        }
      } else if (startx > bug->x) {
        if (g_worldCell->type[screen_pos] == FOOD) {
          right += FOOD_HEALTH;
        } else if (g_worldCell->type[screen_pos] == POISON) {
          right -= POISON_COST;
        } else if (g_worldCell->type[screen_pos] == BUG) {
          right += bug->aggr + bug->drive;
        }
      }

      // Vertical contributions
      if (starty < bug->y) {
        if (g_worldCell->type[screen_pos] == FOOD) {
          up += FOOD_HEALTH;
        } else if (g_worldCell->type[screen_pos] == POISON) {
          up -= POISON_COST;
        } else if (g_worldCell->type[screen_pos] == BUG) {
          up += bug->aggr + bug->drive;
        }
      } else if (starty > bug->y) {
        if (g_worldCell->type[screen_pos] == FOOD) {
          down += FOOD_HEALTH;
        } else if (g_worldCell->type[screen_pos] == POISON) {
          down -= POISON_COST;
        } else if (g_worldCell->type[screen_pos] == BUG) {
          down += bug->aggr + bug->drive;
        }
      }

      // Optionally, handle cells exactly at bug->x and bug->y as needed.
    }
  }

  // Use softmax for horizontal and vertical probabilities.
  double alpha = 0.01;
  double weightL = exp(alpha * left);
  double weightR = exp(alpha * right);
  double pLeft = weightL / (weightL + weightR);

  double weightU = exp(alpha * up);
  double weightD = exp(alpha * down);
  double pUp = weightU / (weightU + weightD);

  // Store probabilities (scaled as percentages)
  *left_prob = (int)(pLeft * 100);
  *up_prob = (int)(pUp * 100);
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
  baby->sex = gsl_rng_get(g_rng) % 2;
  baby->isAlive = 1;
  baby->age = 0;
  // perform  a mutation
  if (gsl_rng_get(g_rng) % 100 >= MUTATION_RATE * 100) {
    int bit = gsl_rng_get(g_rng) % 32;
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
    if (gsl_rng_get(g_rng) % 10000 < INIT_BUG_PROB * 10000) {
      bugs = immaculateBirthABug(i, bugs);
    } else if (gsl_rng_get(g_rng) % 100 < INIT_FOOD_PROB * 100) {
      g_worldCell->type[i] = FOOD;
      g_worldCell->color[i] = FOOD_COLOR;
      g_worldCell->color[i].a = FOOD_OPACITY;
    } else if (gsl_rng_get(g_rng) % 1000 < INIT_POISON_PROB * 1000) {
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
  g_rng = gsl_rng_alloc(gsl_rng_mt19937);
  gsl_rng_set(g_rng, time(NULL));

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
  MaximizeWindow();
  SetTargetFPS(120);

  // Initialize random number generator
  // srand(time(NULL));
  // srand(1234);

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
        screen_pos = bugMove(&bugs[i]);
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
            if (bugs[i].drive > gsl_rng_get(g_rng) % 16 &&
                bugs[i].health > MATING_COST &&
                bugs[g_worldCell->bug_idx[screen_pos]].health > MATING_COST &&
                bugs[i].age >= MIN_MATING_AGE &&
                bugs[g_worldCell->bug_idx[screen_pos]].age >= MIN_MATING_AGE) {
              bugs = birthABug(bugs, i, g_worldCell->bug_idx[screen_pos],
                               screen_pos, old_screen_pos);
            }
          } else if (bugs[i].aggr > gsl_rng_get(g_rng) % 16 &&
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
      // regenerate food and poison
      for (int i = 0; i < WORLD_WIDTH * WORLD_HEIGHT; ++i) {
        if (g_worldCell->type[i] == EMPTY) {
          if (gsl_rng_get(g_rng) % 10000 < REGENERATE_FOOD_RATE * 10000) {
            g_worldCell->type[i] = FOOD;
            g_worldCell->color[i] = FOOD_COLOR;
            g_worldCell->color[i].a = FOOD_OPACITY;
          } else if (gsl_rng_get(g_rng) % 10000 <
                     REGENERATE_POISON_RATE * 10000) {
            g_worldCell->type[i] = POISON;
            g_worldCell->color[i] = RED;
          }
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
