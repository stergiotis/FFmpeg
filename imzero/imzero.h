#pragma once
#include <stdlib.h>
#include <stddef.h>
#include <SDL.h>
#include <SDL_thread.h>

typedef struct ImZeroState {
    void *buffer;
    size_t size;
} ImZeroState;

struct VideoState;
static inline void event_loop_handle_event_imzero(struct VideoState *cur_stream,SDL_Event *event, double *incr, double *pos, double *frac);
static ImZeroState* imzero_init(const char *user_interaction_path);
static void imzero_reset(ImZeroState *state);
static void imzero_destroy(ImZeroState *state);