#pragma once
#ifndef EDITOR_UTILS_H
#define EDITOR_UTILS_H

#include <GameObject.h>

inline const char* boolToString(bool boolean)
{
	if (boolean) { return "True"; }
	return "false";
}

inline const char* objectTypeToString(int type)
{
	switch (type)
	{
	case OBJECT_PLAYER:      return "Player";
	case OBJECT_ENTITY:      return "Entity";
	case OBJECT_ITEM:        return "Item";
	case OBJECT_PROJECTILE:  return "Projectile";
	case OBJECT_ENVIRONMENT: return "Environment";
	default:                 return "Generic";
	}
}

#endif
