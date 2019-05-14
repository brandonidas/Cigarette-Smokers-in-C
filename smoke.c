#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__)
#else
#define VERBOSE_PRINT(S, ...) ((void) 0) // do nothing
#endif


struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

uthread_mutex_t mx;

uthread_cond_t match_paper;
uthread_cond_t paper_tob;
uthread_cond_t match_tob;

int gotMatch = 0;
int gotPaper = 0;
int gotTobacco = 0;

void resetAllGotFlags(){
  gotMatch = 0;
  gotPaper = 0;
  gotTobacco = 0;
}

/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};

  srandom(time(NULL));

  uthread_mutex_lock (a->mutex);

  for (int i = 0; i < NUM_ITERATIONS; i++) {
    //printf("inside random assignment\n");
    int r = random() % 3;
    signal_count [matching_smoker [r]] ++;
    int c = choices [r];
    // printf("c = %d\n", c);
    if (c & MATCH) {
      VERBOSE_PRINT ("match available\n");
      uthread_cond_signal (a->match);
      // printf("signal match\n");
    }
    if (c & PAPER) {
      VERBOSE_PRINT ("paper available\n");
      uthread_cond_signal (a->paper);
      // printf("signal paper\n");
    }
    if (c & TOBACCO) {
      VERBOSE_PRINT ("tobacco available\n");
      uthread_cond_signal (a->tobacco);
      // printf("signal tobacco\n");
    }
    VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
    uthread_cond_wait (a->smoke);
  }

  uthread_mutex_unlock (a->mutex);
  return NULL;
}

void act_smoke(){
  //printf("inside act_smoke\n");
  if(gotMatch & gotPaper){ // 3
    //printf("signal reached\n");
    resetAllGotFlags();
    uthread_cond_signal(match_paper);
  } else if (gotPaper & gotTobacco){ // 6
    //printf("signal reached\n");
    resetAllGotFlags();
    uthread_cond_signal(paper_tob);
  } else if(gotMatch & gotTobacco){ // 5
    //printf("signal reached\n");
    resetAllGotFlags();
    uthread_cond_signal(match_tob);
  } else {
    // Do nothing
    //printf("act_smoke nothing reached\n");
  }
}

// >>>>>> OBSERVERS <<<<<<

void* match_listener(void* av){
  struct Agent* a = av;
  uthread_mutex_lock (a->mutex);
  while(1){ // Continuously polling for signal
    uthread_cond_wait (a->match);
    gotMatch = 1;
    act_smoke();
    // printf("MATCH\n");
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

void* tob_listener(void* av){
  struct Agent* a = av;
  uthread_mutex_lock (a->mutex);
  while(1){ // Continuously polling for signal
    uthread_cond_wait (a->tobacco);
    gotTobacco = 1;
    act_smoke();
    // printf("TOB\n");
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

void* paper_listener(void* av){
  struct Agent* a = av;
  uthread_mutex_lock (a->mutex);
  while(1){
    uthread_cond_wait(a->paper);
    gotPaper = 1;
    act_smoke();
    // printf("PAPER\n");
  }
  uthread_mutex_unlock(a->mutex);
  return NULL;
}

// >>>>>> ACTORS <<<<<<<<

void* actorWithTobacco(void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(match_paper);
    uthread_cond_signal(a->smoke);
    smoke_count[TOBACCO]++;
  }
  uthread_mutex_unlock (a->mutex);
  //return NULL;
}

void* actorWithMatch(void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(paper_tob);
    uthread_cond_signal(a->smoke);
    smoke_count[MATCH]++;
  }
  uthread_mutex_unlock (a->mutex);
  //return NULL;
}

void* actorWithPaper(void* av){
  struct Agent* a = av;
  uthread_mutex_lock(a->mutex);
  while(1){
    uthread_cond_wait(match_tob);
    uthread_cond_signal(a->smoke);
    smoke_count[PAPER]++;
  }
  uthread_mutex_unlock (a->mutex);
  //return NULL;
}

int main (int argc, char** argv) {
  uthread_init (7);
  struct Agent*  a = createAgent();

  mx = uthread_mutex_create();
  match_paper = uthread_cond_create(a->mutex);
  paper_tob = uthread_cond_create(a->mutex);
  match_tob = uthread_cond_create(a->mutex);

  uthread_mutex_lock(mx);
  uthread_t match_t = uthread_create(match_listener,a);
  uthread_t tob_t = uthread_create(tob_listener,a);
  uthread_t paper_t = uthread_create(paper_listener,a);
  uthread_mutex_unlock(mx);

  uthread_mutex_lock(mx);
  uthread_t tobActor_t = uthread_create(actorWithTobacco,a);
  uthread_t paperActor_t = uthread_create(actorWithPaper,a);
  uthread_t matchActor_t = uthread_create(actorWithMatch,a);
  uthread_mutex_unlock(mx);

  uthread_join (uthread_create (agent, a), 0);

  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);

  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}
