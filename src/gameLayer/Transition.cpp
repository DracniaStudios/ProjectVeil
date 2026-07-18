/**
 * @file transition.c
 *
 * @author Killax-D | Dylan DONNE
 *
 * @brief Transition is an animation when switching between two Scene
 *
 * This file contains all declarations and function regarding Transition
 *
 */

 #include "Transition.h"

Transition * Transition_new() {
	Transition * transition = (Transition*)malloc(sizeof(Transition));
	transition->direction = NONE;
	transition->opacity = -1;
	return transition;
}