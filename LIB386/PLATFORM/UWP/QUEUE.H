#pragma once

/* The UWP backend's event queue, free of Windows headers so host tests can
   check it. Window handlers push; the engine polls. One thread does both. */

#include <PLATFORM/PLATFORM.H>

#include <string.h>

enum { UWP_QUEUE_SIZE = 256,
       UWP_TEXT_SIZE = 8 };

typedef struct UwpQueue {
    PlatformEvent events[UWP_QUEUE_SIZE];
    char text[UWP_QUEUE_SIZE][UWP_TEXT_SIZE];
    char textOut[UWP_TEXT_SIZE]; /* what a polled text event points at */
    unsigned head, count;
    /* A quit that found the queue full. The app stops reading window events
       after Closed, so a lost quit would leave the game running forever. */
    bool quit;
} UwpQueue;

static inline void UwpQueueClear(UwpQueue *q) {
    q->head = q->count = 0;
    q->quit = false;
}

/* False when the queue is full and the event was dropped. A quit is never
   dropped: it waits until the queue drains. */
static inline bool UwpQueuePush(UwpQueue *q, const PlatformEvent *event, const char *text) {
    if (q->count == UWP_QUEUE_SIZE) {
        if (event->type != PLATFORM_EVENT_QUIT)
            return false;
        q->quit = true;
        return true;
    }
    const unsigned slot = (q->head + q->count) % UWP_QUEUE_SIZE;
    q->events[slot] = *event;
    q->text[slot][0] = '\0';
    if (text) {
        strncpy(q->text[slot], text, UWP_TEXT_SIZE - 1);
        q->text[slot][UWP_TEXT_SIZE - 1] = '\0';
    }
    ++q->count;
    return true;
}

/* A text event's text is borrowed until the next poll. */
static inline bool UwpQueuePoll(UwpQueue *q, PlatformEvent *event) {
    if (q->count == 0) {
        if (!q->quit)
            return false;
        q->quit = false;
        memset(event, 0, sizeof *event);
        event->type = PLATFORM_EVENT_QUIT;
        return true;
    }
    *event = q->events[q->head];
    if (event->type == PLATFORM_EVENT_TEXT_INPUT) {
        memcpy(q->textOut, q->text[q->head], UWP_TEXT_SIZE);
        event->text.text = q->textOut;
    }
    q->head = (q->head + 1) % UWP_QUEUE_SIZE;
    --q->count;
    return true;
}
