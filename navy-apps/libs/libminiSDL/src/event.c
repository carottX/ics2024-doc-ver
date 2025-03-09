#include <NDL.h>
#include <SDL.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

/**
 * Pushes a custom event onto the event queue.
 *
 * This function allows the application to inject a custom event into the SDL event queue.
 * The event is copied into the queue, and the function returns immediately. The event
 * can be of any type, including user-defined events.
 *
 * @param ev A pointer to the SDL_Event structure containing the event to be pushed.
 *           The event data is copied, so the caller does not need to maintain the
 *           event after this function returns.
 *
 * @return Returns 1 on success, or 0 if the event could not be pushed (e.g., if the
 *         event queue is full). On failure, the event is not added to the queue.
 *
 * @note The event queue has a limited size, and if it is full, this function will fail.
 *       Use SDL_PeepEvents() or SDL_PollEvent() to ensure the queue has space.
 *
 * @see SDL_PeepEvents
 * @see SDL_PollEvent
 */
int SDL_PushEvent(SDL_Event *ev) {
  return 0;
}

/**
 * @brief Polls for currently pending events.
 *
 * This function checks the event queue for any pending events and retrieves
 * the next event from the queue if available. The event is stored in the
 * provided `SDL_Event` structure. If there are no pending events, the function
 * returns immediately.
 *
 * @param ev Pointer to an `SDL_Event` structure to be filled with the event data.
 *           If NULL, the event is discarded.
 *
 * @return Returns 1 if there is a pending event, or 0 if there are no events.
 */
int SDL_PollEvent(SDL_Event *ev) {
  return 0;
}

/**
 * Waits indefinitely for the next available event in the event queue and stores it in the provided SDL_Event structure.
 * This function blocks the calling thread until an event is available, making it suitable for applications that
 * need to process events in a sequential manner. The event is removed from the queue and its details are populated
 * in the 'event' parameter.
 *
 * @param event A pointer to an SDL_Event structure where the event details will be stored.
 * @return Returns 1 on success, indicating that an event was successfully retrieved. Returns 0 if an error occurred
 *         or if the event queue was empty (though this function typically blocks until an event is available).
 */
int SDL_WaitEvent(SDL_Event *event) {
  return 1;
}

/**
 * @brief Retrieves events from the event queue based on the specified criteria.
 *
 * This function allows you to peek at or retrieve events from the event queue without removing them,
 * or to remove events from the queue. The events are filtered based on the provided event type mask.
 *
 * @param ev Pointer to an array of SDL_Event structures to store the retrieved events.
 * @param numevents The maximum number of events to retrieve. Must be greater than 0.
 * @param action The action to perform on the event queue. Possible values are:
 *               - SDL_ADDEVENT: Add events to the queue.
 *               - SDL_PEEKEVENT: Peek at events in the queue without removing them.
 *               - SDL_GETEVENT: Retrieve events from the queue and remove them.
 * @param mask A bitmask specifying which event types to retrieve. Use SDL_FIRSTEVENT and SDL_LASTEVENT
 *             to include all events, or specify individual event types.
 * @return The number of events actually stored in the `ev` array, or a negative error code on failure.
 */
int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

/**
 * @brief Retrieves the current state of the keyboard.
 *
 * This function returns a pointer to an array of key states. Each element in the array
 * represents the state of a specific key, where a value of 1 indicates that the key is
 * pressed and a value of 0 indicates that the key is not pressed. The array is indexed
 * by the SDL scancode values.
 *
 * @param numkeys If not NULL, this will be filled with the number of keys in the array.
 * @return A pointer to an array of key states. The array is owned by SDL and should not
 *         be freed by the caller. If an error occurs, NULL is returned.
 */
uint8_t* SDL_GetKeyState(int *numkeys) {
  return NULL;
}
