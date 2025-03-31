/**
 * @file
 * @brief Spell casting functions.
**/

#pragma once

#include <vector>
#include <memory>
#include <functional>

#include "enum.h"
#include "item-def.h"

using std::vector;


#define fail_check() if (fail) return spret::fail

int list_weapons(bool toggle_with_I = true, bool viewing = false,
    bool allow_preselect = true,
    const string &title = "weild");
void inspect_weapons();
int calc_damage(const item_def &weapon);
int calc_weapon_accuracy(const item_def &weapon);
int calc_weapon_speed(const item_def &weapon);