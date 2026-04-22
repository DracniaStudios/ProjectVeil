/**
 * @file transition.h
 *
 * @author Killax-D | Dylan DONNE
 *
 * @brief Transition is an animation when switching between two Scene
 *
 * This file contains all declarations and function regarding Transition
 *
 */

#ifndef TRANSITION_H
#define TRANSITION_H

#include <stdlib.h>

typedef enum {NONE, IN, OUT} TransitionDirection;

/**
 * Struct to represent a Transition
 */
typedef struct Transition {
	TransitionDirection direction; /**< If transitionDirection is IN or OUT a transition is running */
	int opacity; /**< Opacity of the black fade */
} Transition;

/**
 * Create a Transition and return the pointer
 * @return A pointer to the Transition
 */
Transition * Transition_new();

#endif