/**
 * @file
 * @brief Spell casting and miscast functions.
**/

#include "AppHdr.h"

#include "weapon-menu.h"

#include <iomanip>
#include <sstream>
#include <cmath>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "beam.h"
#include "chardump.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "database.h"
#include "describe.h"
#include "directn.h"
#include "english.h"
#include "env.h"
#include "evoke.h"
#include "exercise.h"
#include "format.h"
#include "god-abil.h"
#include "god-conduct.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::shadow_spells
#include "hints.h"
#include "items.h"
#include "item-prop.h"
#include "item-use.h"
#include "libutil.h"
#include "macro.h"
#include "menu.h"
#include "melee-attack.h"
#include "message.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-explode.h"
#include "mon-place.h"
#include "mon-project.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-monench.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "spl-zap.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "tag-version.h"
#include "target.h"
#include "teleport.h"
#include "terrain.h"
#include "tilepick.h"
#include "transform.h"
#include "unicode.h"
#include "unwind.h"
#include "view.h"
#include "viewchar.h" // stringize_glyph


class WeaponMenuEntry : public ToggleableMenuEntry
{
public:
    WeaponMenuEntry(const string &txt,
                   const string &alt_txt,
                   MenuEntryLevel lev,
                   int qty, int hotk, int indx)
        : ToggleableMenuEntry(txt, alt_txt, lev, qty, hotk)
    {
        index = indx;
    }

    bool preselected = false;
    int index = 0;
protected:
    virtual string _get_text_preface() const override
    {
        if (preselected)
            return make_stringf(" %s + ", keycode_to_name(hotkeys[0]).c_str());
        return ToggleableMenuEntry::_get_text_preface();
    }
};

class WeaponMenu : public ToggleableMenu
{
public:
    WeaponMenu()
        : ToggleableMenu(MF_SINGLESELECT | MF_ANYPRINTABLE
            | MF_NO_WRAP_ROWS | MF_ALLOW_FORMATTING
            | MF_ARROWS_SELECT | MF_INIT_HOVER) {}
protected:

    bool process_command(command_type c) override
    {
        get_selected(&sel);
        // if there's a preselected item, and no current selection, select it.
        // for arrow selection, the hover starts on the preselected item so no
        // special handling is needed.
        if (menu_action == ACT_EXECUTE && c == CMD_MENU_SELECT
            && !(flags & MF_ARROWS_SELECT) && sel.empty())
        {
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (static_cast<WeaponMenuEntry*>(items[i])->preselected)
                {
                    select_index(i, 1);
                    break;
                }
            }
        }
        return ToggleableMenu::process_command(c);
    }

    bool examine_index(int i) override
    {
        ASSERT(i >= 0 && i < static_cast<int>(items.size()));
        if (items[i]->hotkeys.size())
            describe_item(you.inv[static_cast<WeaponMenuEntry*>(items[i])->index]);
        return true;
    }
};


static string _weapon_base_description(const item_def &weapon, bool viewing)
{
    ostringstream desc;

    // spell name
    desc << chop_string(weapon.name(DESC_INVENTORY), 32);

    // spell schools
    desc << chop_string(to_string(get_weapon_brand(weapon)),8);

    // XXX: This exact padding is needed to make the webtiles spell menu not re-align
    //      itself whenever we toggle display modes. For some reason, this doesn't
    //      seem to matter for local tiles. Who know why?
    desc << "      ";

    return desc.str();
}

static string _weapon_extra_description(const item_def &weapon, bool viewing)
{
    ostringstream desc;

    // spell name
    desc << chop_string(weapon.name(DESC_INVENTORY), 32);

    // spell power, spell range, noise
    const string damagestring = to_string(calc_damage(weapon));
    const string speedstring = to_string(calc_weapon_speed(weapon));

    desc << chop_string(to_string(calc_weapon_accuracy(weapon)), 10)
         << chop_string(damagestring.length() ? damagestring : "N/A", 10)
         << chop_string(speedstring, 8);

    return desc.str();
}

int list_weapons(bool toggle_with_I, bool viewing, bool allow_preselect,
    const string &action)
{
    if (toggle_with_I && get_spell_by_letter('I') != SPELL_NO_SPELL)
    toggle_with_I = false;

    const string real_action = viewing ? "describe" : action;

    WeaponMenu weapon_menu;
    const string titlestring = make_stringf("%-25.25s",
    make_stringf("Your Weapons (%s)", real_action.c_str()).c_str());

    {
        ToggleableMenuEntry* me =
        new ToggleableMenuEntry(
        titlestring + "       Brand     Other                     ",
        titlestring + "    Accuracy     Damage     Speed           ",
        MEL_TITLE);
    weapon_menu.set_title(me, true, true);
    }
    weapon_menu.set_highlighter(nullptr);
    weapon_menu.set_tag("weapon");
    // TODO: add toggling to describe mode with `?`, add help string, etc...
    weapon_menu.add_toggle_from_command(CMD_MENU_CYCLE_MODE);
    weapon_menu.add_toggle_from_command(CMD_MENU_CYCLE_MODE_REVERSE);

    string more_str = make_stringf("<lightgrey>Select a weapon to %s</lightgrey>",
    real_action.c_str());
    string toggle_desc = menu_keyhelp_cmd(CMD_MENU_CYCLE_MODE);
    if (toggle_with_I)
    {
// why `I`?
        weapon_menu.add_toggle_key('I');
        toggle_desc += "/[<w>I</w>]";
    }
    toggle_desc += " toggle weapon headers";

    more_str = pad_more_with(more_str, toggle_desc);
    weapon_menu.set_more(formatted_string::parse_string(more_str));
    // TODO: should allow toggling between execute and examine
    weapon_menu.menu_action = viewing ? Menu::ACT_EXAMINE : Menu::ACT_EXECUTE;

    int initial_hover = 0; 
    for (int i = 0; i < 52; i++)
    {
        int letter = index_to_letter(i);
        if(!you.inv[i].is_valid())
        {
            continue;
        }
        if (you.inv[i].base_type != OBJ_WEAPONS)
        {
            continue;
        }
        //string brand = chop_string(to_string(get_weapon_brand(you.inv[i])),8);
        //int damage = calc_damage(you.inv[i]);
        //int accuracy = calc_weapon_accuracy(you.inv[i]);
        //int speed = calc_weapon_speed(you.inv[i]);
        WeaponMenuEntry* me =
        new WeaponMenuEntry(_weapon_base_description(you.inv[i], viewing),
                    _weapon_extra_description(you.inv[i], viewing),
                    MEL_ITEM, 1, letter,i);
    // TODO: maybe fill this from the quiver if there's a quivered spell and


        me->add_tile(tile_def(tileidx_item(you.inv[i])));
        weapon_menu.add_entry(me);

    }
    weapon_menu.set_hovered(initial_hover);

    int choice = 0;
    weapon_menu.on_single_selection = [&choice](const MenuEntry& item)
    {
        ASSERT(item.hotkeys.size() == 1);
    choice = item.hotkeys[0];
    return false;
    };

    weapon_menu.show();
    if (!crawl_state.doing_prev_cmd_again)
    {
    redraw_screen();
    update_screen();
    }
    return choice;
}

void inspect_weapons()
{
    list_weapons(true, true);
}

int calc_damage(const item_def &weapon)
{
    int base = property(weapon, PWPN_DAMAGE);  // Base weapon damage
    int enchant = weapon.plus;                 // Enchantment bonus

    int total_damage = base + enchant;
    return total_damage;
}
int calc_weapon_accuracy(const item_def &weapon)
{
    int base_accuracy = property(weapon, PWPN_HIT);  // Base accuracy of the weapon
    int enchant = weapon.plus;                       // Enchantment bonus

    int total_accuracy = base_accuracy + enchant;
    return total_accuracy;
}
int calc_weapon_speed(const item_def &weapon)
{
    int base_speed = property(weapon, PWPN_SPEED);  // Base weapon speed
    int skill_level = you.skill(SK_FIGHTING);        // Weapon skill level

    // Skill reduces delay by a factor, capped at a minimum delay.
    int speed_reduction = std::min(skill_level / 2, 7);
    int effective_speed = std::max(base_speed - speed_reduction, 7);

    return effective_speed;
}


