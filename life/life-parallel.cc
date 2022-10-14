#include "life.h"
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <iostream>
using namespace std;

struct Section {
    int start_row;
    int start_col;
    int end_row;
    int end_col;
};

struct Args {
    LifeBoard *state1;
    LifeBoard *state2;
    Section section;
    int steps;
    int width;
    pthread_barrier_t *barrier;
};

Section make_section(int first_cell, int last_cell, int width) {
    Section section;
    section.start_row = first_cell / (width - 2) + 1;
    section.start_col = first_cell % (width - 2) + 1;
    section.end_row = last_cell / (width - 2) + 1;
    section.end_col = last_cell % (width - 2) + 1;
    return section;
}

vector<Section> divide_board(int threads, int width, int height) {
    vector<Section> sections;
    int cells = (width - 2) * (height - 2);
    int section_size = ceil((float)cells / (float)threads);
    for (int cell = 0; cell < cells; cell += section_size) {
        sections.push_back(make_section(cell, min(cell + section_size, cells) - 1, width));
    }
    return sections;
}

void next_generation(LifeBoard &cur_state, LifeBoard &next_state, struct Section &section, int width) {
    for (int y = section.start_row; y <= section.end_row; ++y) {
        int col_start = 1;
        if(y == section.start_row){
            col_start = section.start_col;
        }
        int col_end = width - 2;
        if(y == section.end_row){
            col_end = section.end_col;
        }
        for (int x = col_start; x <= col_end; ++x) {
            int live_in_window = 0;
            /* For each cell, examine a 3x3 "window" of cells around it,
                * and count the number of live (true) cells in the window. */
            for (int y_offset = -1; y_offset <= 1; ++y_offset) {
                for (int x_offset = -1; x_offset <= 1; ++x_offset) {
                    if (cur_state.at(x + x_offset, y + y_offset)) {
                        ++live_in_window;
                    }
                }
            }
            /* Cells with 3 live neighbors remain or become live.
                Live cells with 2 live neighbors remain live. */
            next_state.at(x, y) = (
                live_in_window == 3 /* dead cell with 3 neighbors or live cell with 2 */ ||
                (live_in_window == 4 && cur_state.at(x, y)) /* live cell with 3 neighbors */
            );
        }
    }
}

void *generate_life(void *args) {
    struct Args *casted_args = (struct Args *)(args);
    for (int step = 0; step < casted_args->steps; ++step) {
        if (step % 2) {
            next_generation(*(casted_args->state2), *(casted_args->state1), casted_args->section, casted_args->width);
        } else {
            next_generation(*(casted_args->state1), *(casted_args->state2), casted_args->section, casted_args->width);
        }
        pthread_barrier_wait(casted_args->barrier);
    }
    delete (struct Args *)args;
    return NULL;
}

void simulate_life_parallel(int threads, LifeBoard &state, int steps) {
    vector<Section> sections = divide_board(threads, state.width(), state.height());
    LifeBoard state2{state.width(), state.height()};

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, threads);

    vector<pthread_t> thread_pool(threads);
    for (int i = 0; i < threads; i++) {
        Args *args = new Args();
        args->state1 = &state;
        args->state2 = &state2;
        args->section = sections[i];
        args->steps = steps;
        args->width = state.width();
        args->barrier = &barrier;
        pthread_create(&thread_pool[i], NULL, generate_life, (void*)args);
    }
    for (int i = 0; i < threads; i++) {
        pthread_join(thread_pool[i], NULL);
    }
    pthread_barrier_destroy(&barrier);
    if (steps % 2) {
        swap(state, state2);
    }
}
