#include <errno.h>
#include <linux/input.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <threads.h>
#include <unistd.h>

typedef struct _controller_positions {
  int32_t left_stick_x;
  int32_t left_stick_y;
  int32_t right_stick_x;
  int32_t right_stick_y;
} cposns;

typedef struct _controller_state {
  char *filename;
  bool shutdown;
  mtx_t mtx; // Guards data
  cposns data;
} cstate;

void *cstate_new(const char *filename) {
  cstate *state = (cstate *)malloc(sizeof(cstate));
  state->shutdown = false;
  state->filename = strdup(filename);
  memset(&state->data, 0, sizeof(state->data));
  if (mtx_init(&state->mtx, mtx_plain | mtx_recursive) != thrd_success) {
    perror("error creating cstate mutex");
    goto err;
  }
  return state;

err:
  free(state->filename);
  free(state);
  return 0;
}

static bool posns_update_from_event(cposns *posns,
                                    const struct input_event *event) {
  switch (event->type) {
  case EV_SYN:
    return true;
  case EV_ABS:
    switch (event->code) {
    case ABS_X:
      posns->left_stick_x = event->value;
      return true;
    case ABS_Y:
      posns->left_stick_y = event->value;
      return true;
    case ABS_RX:
      posns->right_stick_x = event->value;
      return true;
    case ABS_RY:
      posns->right_stick_y = event->value;
      return true;
    }
  }
  return false;
}

int cstate_update_thread(void *_cstate) {
  cstate *state = (cstate *)_cstate;

  cposns posns_copy;
  {
    mtx_lock(&state->mtx);
    memcpy(&posns_copy, &state->data, sizeof(posns_copy));
    mtx_unlock(&state->mtx);
  }

  FILE *file = fopen(state->filename, "r");
  if (!file) {
    perror("error opening input event file");
    return 1;
  }

  struct input_event events[20];
  bool shutdown = false;
  while (!shutdown) {
    // TODO: jaguilar - use poll instead
    const int nread = read(fileno(file), &events, sizeof(events));
    if (nread < 0) {
      perror("error reading from fd");
      return 1;
    } else if (nread == 0) {
      perror("EOF");
      return 1;
    }
    const int nevents = nread / sizeof(struct input_event);
    for (int i = 0; i < nevents; ++i) {
      if (!posns_update_from_event(&posns_copy, &events[i])) {
        // Print unhandled event.
        printf("Unhandled event: type: %d, code: %d, value: %d\n",
               events[i].type, events[i].code, events[i].value);
      }
    }
    {
      mtx_lock(&state->mtx);
      memcpy(&state->data, &posns_copy, sizeof(posns_copy));
      shutdown = state->shutdown;
      mtx_unlock(&state->mtx);
    }
  }
  return 0;
}

void cstate_get_positions(void *vstate, void *vposns) {
  cstate *state = (cstate *)vstate;
  cposns *posns = (cposns *)vposns;
  mtx_lock(&state->mtx);
  memcpy(posns, &state->data, sizeof(*posns));
  mtx_unlock(&state->mtx);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("first argument should be a /dev/input/event file\n");
    return 1;
  }

  const char *filename = argv[1];
  cstate *state = cstate_new(filename);

  thrd_t t1;
  if (thrd_create(&t1, cstate_update_thread, state) != thrd_success) {
    perror("error creating thread");
    return 1;
  };

  cposns posns;
  while (true) {
    sleep(1);
    cstate_get_positions(state, &posns);
    printf("left_stick_x: %d, left_stick_y: %d, right_stick_x: %d, "
           "right_stick_y: %d\n",
           posns.left_stick_x, posns.left_stick_y, posns.right_stick_x,
           posns.right_stick_y);
  }

  return 0;
}