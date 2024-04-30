// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "sc_demon_hunter.hpp"

#include "class_modules/apl/apl_demon_hunter.hpp"

// ==========================================================================
// Demon Hunter
// ==========================================================================

namespace demon_hunter
{
namespace actions
{
// ==========================================================================
// Demon Hunter Actions
// ==========================================================================
// Template for common demon hunter action code. See priest_action_t.

template <class Base>
demon_hunter_action_t<Base>::demon_hunter_action_t( util::string_view n, demon_hunter_t *p, const spell_data_t *s,
                                                    util::string_view o )
  : ab( n, p, s ),
    energize_delta( 0.0 ),
    track_cd_waste( s->cooldown() > timespan_t::zero() || s->charge_cooldown() > timespan_t::zero() ),
    cd_wasted_exec( nullptr ),
    cd_wasted_cumulative( nullptr ),
    cd_wasted_iter( nullptr )
{
  ab::parse_options( o );
}

template <class Base>
void demon_hunter_action_t<Base>::parse_affect_flags( const spell_data_t *spell, affect_flags &flags )
{
  for ( const spelleffect_data_t &effect : spell->effects() )
  {
    if ( !effect.ok() || effect.type() != E_APPLY_AURA || effect.subtype() != A_ADD_PCT_MODIFIER )
      continue;

    if ( ab::data().affected_by( effect ) )
    {
      switch ( effect.misc_value1() )
      {
        case P_GENERIC:
          flags.direct = true;
          break;
        case P_TICK_DAMAGE:
          flags.periodic = true;
          break;
      }
    }
  }
}

template <class Base>
demon_hunter_t *demon_hunter_action_t<Base>::p()
{
  return debug_cast<demon_hunter_t *>( ab::player );
}

template <class Base>
const demon_hunter_t *demon_hunter_action_t<Base>::p() const
{
  return debug_cast<demon_hunter_t *>( ab::player );
}

template <class Base>
demon_hunter_td_t *demon_hunter_action_t<Base>::get_td( player_t *target ) const
{
  return p()->get_target_data( target );
}

template <class Base>
const demon_hunter_td_t *demon_hunter_action_t<Base>::find_td( player_t *target ) const
{
  return p()->find_target_data( target );
}

template <class Base>
void demon_hunter_action_t<Base>::apply_buff_effects()
{
}

template <class Base>
void demon_hunter_action_t<Base>::apply_debuff_effects()
{
}

// The only things that really should go in here are calls to the defined
// `register_damage_buff` function.
template <class Base>
void demon_hunter_action_t<Base>::register_damage_buffs()
{
  auto register_damage_buff = [ this ]( damage_buff_t *buff ) {
    if ( buff->is_affecting_direct( ab::s_data ) )
      direct_damage_buffs.push_back( buff );

    if ( buff->is_affecting_periodic( ab::s_data ) )
      periodic_damage_buffs.push_back( buff );

    if ( ab::repeating && !ab::special && !ab::s_data->ok() && buff->auto_attack_mod.multiplier != 1.0 )
      auto_attack_damage_buffs.push_back( buff );

    if ( buff->is_affecting_crit_chance( ab::s_data ) )
      crit_chance_buffs.push_back( buff );
  };

  direct_damage_buffs.clear();
  periodic_damage_buffs.clear();
  auto_attack_damage_buffs.clear();
  crit_chance_buffs.clear();

  register_damage_buff( p()->buff.demon_soul );
  register_damage_buff( p()->buff.empowered_demon_soul );
  register_damage_buff( p()->buff.momentum );
  register_damage_buff( p()->buff.inertia );
  register_damage_buff( p()->buff.restless_hunter );
  register_damage_buff( p()->buff.t29_havoc_4pc );
  register_damage_buff( p()->buff.t31_vengeance_2pc );
  register_damage_buff( p()->buff.glaive_flurry );
  register_damage_buff( p()->buff.rending_strike );
  register_damage_buff( p()->buff.enduring_torment );
}

template <class Base>
void demon_hunter_action_t<Base>::parse_all_affect_flags()
{
  if ( p()->specialization() == DEMON_HUNTER_HAVOC )
  {
    parse_affect_flags( p()->mastery.demonic_presence, affected_by.demonic_presence );
    parse_affect_flags( p()->mastery.any_means_necessary, affected_by.any_means_necessary );

    if ( p()->talent.havoc.essence_break->ok() )
    {
      affected_by.essence_break = ab::data().affected_by( p()->spec.essence_break_debuff );
    }

    if ( p()->spec.burning_wound_debuff->ok() )
    {
      affected_by.burning_wound = ab::data().affected_by( p()->spec.burning_wound_debuff->effectN( 2 ) );
    }

    if ( p()->talent.havoc.chaos_theory->ok() )
    {
      affected_by.chaos_theory = ab::data().affected_by( p()->spec.chaos_theory_buff->effectN( 1 ) );
    }

    if ( p()->talent.havoc.serrated_glaive->ok() )
    {
      affected_by.serrated_glaive = ab::data().affected_by( p()->spec.serrated_glaive_debuff );
    }
  } else {
    if ( p()->talent.vengeance.vulnerability->ok() )
    {
      affected_by.frailty = ab::data().affected_by( p()->spec.frailty_debuff->effectN( 4 ) );
    }
    if ( p()->talent.vengeance.fiery_demise->ok() )
    {
      affected_by.fiery_demise = ab::data().affected_by( p()->spec.fiery_brand_debuff->effectN( 2 ) ) ||
                                 ( p()->set_bonuses.t30_vengeance_4pc->ok() &&
                                   ab::data().affected_by_label( p()->spec.fiery_brand_debuff->effectN( 4 ) ) );
    }
    if ( p()->set_bonuses.t30_vengeance_2pc->ok() )
    {
      affected_by.fires_of_fel = ab::data().affected_by( p()->set_bonuses.t30_vengeance_2pc_buff->effectN( 1 ) ) ||
                                 ab::data().affected_by( p()->set_bonuses.t30_vengeance_2pc_buff->effectN( 2 ) );
    }
  }

  if ( p()->talent.aldrachi_reaver.art_of_the_glaive->ok() )
  {
    affected_by.reavers_mark = ab::data().affected_by( p()->hero_spec.reavers_mark->effectN( 1 ) );
  }
}

// Utility function to search spell data for matching effect.
// NOTE: This will return the FIRST effect that matches parameters.
template <class Base>
const spelleffect_data_t *demon_hunter_action_t<Base>::find_spelleffect( const spell_data_t *spell,
                                                                         effect_subtype_t subtype, int misc_value,
                                                                         const spell_data_t *affected,
                                                                         effect_type_t type )
{
  for ( size_t i = 1; i <= spell->effect_count(); i++ )
  {
    const auto &eff = spell->effectN( i );

    if ( affected->ok() && !affected->affected_by_all( eff ) )
      continue;

    if ( eff.type() == type && eff.subtype() == subtype )
    {
      if ( misc_value != 0 )
      {
        if ( eff.misc_value1() == misc_value )
          return &eff;
      }
      else
        return &eff;
    }
  }

  return &spelleffect_data_t::nil();
}

// Return the appropriate spell when `base` is overridden to another spell by `passive`
template <class Base>
const spell_data_t *demon_hunter_action_t<Base>::find_spell_override( const spell_data_t *base,
                                                                      const spell_data_t *passive )
{
  if ( !passive->ok() )
    return base;

  auto id = as<unsigned>( find_spelleffect( passive, A_OVERRIDE_ACTION_SPELL, base->id() )->base_value() );
  if ( !id )
    return base;

  return find_spell( id );
}

template <class Base>
bool demon_hunter_action_t<Base>::ready()
{
  if ( ( ab::execute_time() > timespan_t::zero() || ab::channeled ) && p()->buff.out_of_range->check() )
  {
    return false;
  }

  if ( p()->buff.out_of_range->check() && ab::range <= 5.0 )
  {
    return false;
  }

  if ( p()->buff.fel_rush_move->check() )
  {
    return false;
  }

  return ab::ready();
}

template <class Base>
void demon_hunter_action_t<Base>::init()
{
  ab::init();

  register_damage_buffs();

  if ( track_cd_waste )
  {
    cd_wasted_exec = p()->template get_data_entry<simple_sample_data_with_min_max_t,
                                                  std::pair<std::string, simple_sample_data_with_min_max_t>>(
        ab::name_str, p()->cd_waste_exec );
    cd_wasted_cumulative = p()->template get_data_entry<simple_sample_data_with_min_max_t,
                                                        std::pair<std::string, simple_sample_data_with_min_max_t>>(
        ab::name_str, p()->cd_waste_cumulative );
    cd_wasted_iter = p()->template get_data_entry<simple_sample_data_t, std::pair<std::string, simple_sample_data_t>>(
        ab::name_str, p()->cd_waste_iter );
  }
}

template <class Base>
void demon_hunter_action_t<Base>::init_finished()
{
  // Update the stats reporting for spells that use background sub-spells
  if ( ab::execute_action && ab::execute_action->school != SCHOOL_NONE )
    ab::stats->school = ab::execute_action->school;
  else if ( ab::impact_action && ab::impact_action->school != SCHOOL_NONE )
    ab::stats->school = ab::impact_action->school;
  else if ( ab::tick_action && ab::tick_action->school != SCHOOL_NONE )
    ab::stats->school = ab::tick_action->school;

  // For reporting purposes only, as the game displays this as SCHOOL_CHAOS
  if ( ab::stats->school == SCHOOL_CHROMATIC )
    ab::stats->school = SCHOOL_CHAOS;
  if ( ab::stats->school == SCHOOL_CHROMASTRIKE )
    ab::stats->school = SCHOOL_CHAOS;

  ab::init_finished();
}

}  // namespace actions
}  // namespace demon_hunter