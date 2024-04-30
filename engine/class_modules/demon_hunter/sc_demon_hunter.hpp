// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#pragma once

#include "action/action_callback.hpp"
#include "action/parse_effects.hpp"
#include "action/residual_action.hpp"
#include "buff/buff.hpp"
#include "dbc/data_enums.hh"
#include "dbc/spell_data.hpp"
#include "player/pet_spawner.hpp"
#include "player/player.hpp"
#include "sc_enums.hpp"
#include "sim/proc.hpp"
#include "util/timeline.hpp"

#include <array>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "simulationcraft.hpp"

namespace demon_hunter
{
struct demon_hunter_t;
struct demon_hunter_td_t;

namespace actions
{

template <class Base>
struct demon_hunter_action_t : public parse_action_effects_t<Base, demon_hunter_t, demon_hunter_td_t>
{
  double energize_delta;

  // Cooldown tracking
  bool track_cd_waste;
  simple_sample_data_with_min_max_t *cd_wasted_exec, *cd_wasted_cumulative;
  simple_sample_data_t* cd_wasted_iter;

  // Affect flags for various dynamic effects
  struct affect_flags
  {
    bool direct   = false;
    bool periodic = false;
  };

  struct
  {
    // Havoc
    affect_flags any_means_necessary;
    affect_flags demonic_presence;
    bool chaos_theory    = false;
    bool essence_break   = false;
    bool burning_wound   = false;
    bool serrated_glaive = false;

    // Vengeance
    bool frailty           = false;
    bool fiery_demise      = false;
    bool fires_of_fel      = false;
    bool t31_vengeance_2pc = true;

    // Aldrachi Reaver
    bool reavers_mark = false;
  } affected_by;

  std::vector<damage_buff_t*> direct_damage_buffs;
  std::vector<damage_buff_t*> periodic_damage_buffs;
  std::vector<damage_buff_t*> auto_attack_damage_buffs;
  std::vector<damage_buff_t*> crit_chance_buffs;

  void parse_affect_flags( const spell_data_t* spell, affect_flags& flags );

private:
  using ab = parse_action_effects_t<Base, demon_hunter_t, demon_hunter_td_t>;

public:
  using base_t = demon_hunter_action_t<Base>;

  demon_hunter_action_t( util::string_view n, demon_hunter_t* p, const spell_data_t* s = spell_data_t::nil(),
                         util::string_view o = {} );
  demon_hunter_t* p();
  const demon_hunter_t* p() const;
  demon_hunter_td_t* get_td( player_t* target ) const;
  const demon_hunter_td_t* find_td( player_t* target ) const;
  void apply_buff_effects();
  void apply_debuff_effects();
  void register_damage_buffs();
  void parse_all_affect_flags();

  template <typename... Ts>
  void parse_effects( Ts &&...args )
  {
    ab::parse_effects( std::forward<Ts>( args )... );
  }
  template <typename... Ts>
  void parse_target_effects( Ts &&...args )
  {
    ab::parse_target_effects( std::forward<Ts>( args )... );
  }

  const spelleffect_data_t *find_spelleffect( const spell_data_t *spell, effect_subtype_t subtype, int misc_value,
                                              const spell_data_t *affected, effect_type_t type );
  const spell_data_t *find_spell_override( const spell_data_t *base, const spell_data_t *passive );

  bool ready() override;
  void init() override;
  void init_finished() override;
};

}  // namespace actions
}  // namespace demon_hunter
