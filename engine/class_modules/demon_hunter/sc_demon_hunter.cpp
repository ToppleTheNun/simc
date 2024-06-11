// ==========================================================================
// Dedmonwakeen's DPS-DPM Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include "sc_demon_hunter.hpp"

#include "class_modules/apl/apl_demon_hunter.hpp"

// ==========================================================================
// Demon Hunter
// ==========================================================================

namespace demon_hunter
{
soul_fragment operator&( soul_fragment l, soul_fragment r )
{
  return static_cast<soul_fragment>( static_cast<unsigned>( l ) & static_cast<unsigned>( r ) );
}

const char* get_soul_fragment_str( soul_fragment type )
{
  switch ( type )
  {
    case soul_fragment::ANY:
      return "soul fragment";
    case soul_fragment::GREATER:
      return "greater soul fragment";
    case soul_fragment::LESSER:
      return "lesser soul fragment";
    case soul_fragment::GREATER_DEMON:
      return "greater demon fragment";
    case soul_fragment::EMPOWERED_DEMON:
      return "empowered demon fragment";
    default:
      return "";
  }
}

namespace actions
{
const std::vector<demonsurge_ability> demonsurge_havoc_abilities{
    demonsurge_ability::SIGIL_OF_DOOM, demonsurge_ability::CONSUMING_FIRE, demonsurge_ability::ABYSSAL_GAZE,
    demonsurge_ability::ANNIHILATION, demonsurge_ability::DEATH_SWEEP };

const std::vector<demonsurge_ability> demonsurge_vengeance_abilities{
    demonsurge_ability::SOUL_SUNDER, demonsurge_ability::SPIRIT_BURST, demonsurge_ability::SIGIL_OF_DOOM,
    demonsurge_ability::CONSUMING_FIRE, demonsurge_ability::FEL_DESOLATION };

std::string demonsurge_ability_name( demonsurge_ability ability )
{
  switch ( ability )
  {
    case demonsurge_ability::SOUL_SUNDER:
      return "demonsurge_soul_sunder";
    case demonsurge_ability::SPIRIT_BURST:
      return "demonsurge_spirit_burst";
    case demonsurge_ability::SIGIL_OF_DOOM:
      return "demonsurge_sigil_of_doom";
    case demonsurge_ability::CONSUMING_FIRE:
      return "demonsurge_consuming_fire";
    case demonsurge_ability::FEL_DESOLATION:
      return "demonsurge_fel_desolation";
    case demonsurge_ability::ABYSSAL_GAZE:
      return "demonsurge_ABYSSAL_GAZE";
    case demonsurge_ability::ANNIHILATION:
      return "demonsurge_annihilation";
    case demonsurge_ability::DEATH_SWEEP:
      return "demonsurge_death_sweep";
    default:
      return "demonsurge_unknown";
  }
}

namespace attacks
{
}

namespace heals
{
}

namespace spells
{
}
}  // namespace actions

namespace events
{
struct exit_melee_event_t : public event_t
{
  demon_hunter_t& dh;
  buffs::movement_buff_t* trigger_buff;

  exit_melee_event_t( demon_hunter_t* p, timespan_t delay, buffs::movement_buff_t* trigger_buff )
    : event_t( *p, delay ), dh( *p ), trigger_buff( trigger_buff )
  {
    assert( delay > timespan_t::zero() );
  }

  const char* name() const override
  {
    return "exit_melee_event";
  }

  void execute() override
  {
    // Trigger an out of range buff based on the distance to return plus remaining movement aura time
    if ( trigger_buff && trigger_buff->yards_from_melee > 0.0 )
    {
      const timespan_t base_duration =
          timespan_t::from_seconds( trigger_buff->yards_from_melee / dh.cache.run_speed() );
      dh.set_out_of_range( base_duration + trigger_buff->remains() );
    }

    dh.exit_melee_event = nullptr;
  }
};
}

namespace buffs
{
movement_buff_t::movement_buff_t( demon_hunter_t* p, util::string_view name, const spell_data_t* spell_data,
                                  const item_t* item )
  : buff_t( p, name, spell_data, item ), yards_from_melee( 0.0 ), distance_moved( 0.0 ), dh( p )
{
}

bool movement_buff_t::trigger( int s, double v, double c, timespan_t d )
{
  assert( distance_moved > 0 );
  assert( buff_duration() > timespan_t::zero() );

  // Check if we're already moving away from the target, if so we will now be moving towards it
  if ( dh->current.distance_to_move || dh->buff.out_of_range->check() || dh->buff.vengeful_retreat_move->check() ||
       dh->buff.metamorphosis_move->check() )
  {
    dh->set_out_of_range( timespan_t::zero() );
    yards_from_melee = 0.0;
  }
  // Since we're not yet moving, we should be moving away from the target
  else
  {
    // Calculate the number of yards away from melee this will send us.
    // This is equal to reach + melee range times the direction factor
    // With 2.0 being moving fully "across" the target and 1.0 moving fully "away"
    yards_from_melee =
        std::max( 0.0, distance_moved - ( ( dh->get_target_reach() + 5.0 ) * dh->options.movement_direction_factor ) );
  }

  if ( yards_from_melee > 0.0 )
  {
    assert( !dh->exit_melee_event );

    // Calculate the amount of time it will take for the movement to carry us out of range
    const timespan_t delay = buff_duration() * ( 1.0 - ( yards_from_melee / distance_moved ) );

    assert( delay > timespan_t::zero() );

    // Schedule event to set us out of melee.
    dh->exit_melee_event = make_event<events::exit_melee_event_t>( *sim, dh, delay, this );
  }

  // TODO -- Make this actually inherit from the base movement_buff_t class
  for ( const auto& cb : player->callbacks_on_movement )
  {
    if ( !check() )
      cb( true );
  }

  return buff_t::trigger( s, v, c, d );
}
} // namespace buffs

namespace expressions
{
struct eye_beam_adjusted_cooldown_expr_t : public expr_t
{
  demon_hunter_t* dh;
  double cooldown_multiplier;

  eye_beam_adjusted_cooldown_expr_t( demon_hunter_t* p, util::string_view name_str )
    : expr_t( name_str ), dh( p ), cooldown_multiplier( 1.0 )
  {
  }

  void calculate_multiplier()
  {
    double reduction_per_second = 0.0;

    if ( dh->talent.havoc.cycle_of_hatred->ok() )
    {
      /* NYI
      action_t* chaos_strike = dh->find_action( "chaos_strike" );
      assert( chaos_strike );

      // Fury estimates are on the conservative end, intended to be rough approximation only
      double approx_fury_per_second = 15.5;

      // Use base haste only for approximation, don't want to calculate with temp buffs
      const double base_haste = 1.0 / dh->collected_data.buffed_stats_snapshot.attack_haste;
      approx_fury_per_second *= base_haste;

      // Assume 90% of Fury used on Chaos Strike/Annihilation
      const double approx_seconds_per_refund = ( chaos_strike->cost() / ( approx_fury_per_second * 0.9 ) )
        / dh->spec.chaos_strike_refund->proc_chance();

      if ( dh->talent.cycle_of_hatred->ok() )
        reduction_per_second += dh->talent.cycle_of_hatred->effectN( 1 ).base_value() / approx_seconds_per_refund;
        */
    }

    cooldown_multiplier = 1.0 / ( 1.0 + reduction_per_second );
  }

  double evaluate() override
  {
    // Need to calculate shoulders on first evaluation because we don't have crit/haste values on init
    if ( cooldown_multiplier == 1 && dh->talent.havoc.cycle_of_hatred->ok() )
    {
      calculate_multiplier();
    }

    return dh->cooldown.eye_beam->remains().total_seconds() * cooldown_multiplier;
  }
};
} // namespace expressions

// ==========================================================================
// Target Data Definitions
// ==========================================================================

demon_hunter_td_t::demon_hunter_td_t( player_t* target, demon_hunter_t& p )
  : actor_target_data_t( target, &p ), dots( dots_t() ), debuffs( debuffs_t() )
{
  if ( p.specialization() == DEMON_HUNTER_HAVOC )
  {
    debuffs.essence_break = make_buff( *this, "essence_break", p.find_spell( 320338 ) )
                                ->set_default_value_from_effect_type( A_MOD_DAMAGE_FROM_CASTER_SPELLS )
                                ->set_refresh_behavior( buff_refresh_behavior::DURATION )
                                ->set_cooldown( timespan_t::zero() );

    debuffs.initiative_tracker =
        make_buff( *this, "initiative_tracker", p.talent.havoc.initiative )->set_duration( timespan_t::min() );

    debuffs.burning_wound = make_buff( *this, "burning_wound", p.spec.burning_wound_debuff )
                                ->set_default_value_from_effect_type( A_MOD_DAMAGE_FROM_CASTER_SPELLS )
                                ->set_refresh_behavior( buff_refresh_behavior::PANDEMIC );
    dots.burning_wound = target->get_dot( "burning_wound", &p );
  }
  else  // DEMON_HUNTER_VENGEANCE
  {
    dots.fiery_brand = target->get_dot( "fiery_brand", &p );
    debuffs.frailty  = make_buff( *this, "frailty", p.spec.frailty_debuff )
                          ->set_default_value_from_effect( 1 )
                          ->set_refresh_behavior( buff_refresh_behavior::DURATION )
                          ->set_stack_behavior( buff_stack_behavior::ASYNCHRONOUS )
                          ->set_period( 0_ms )
                          ->apply_affecting_aura( p.talent.vengeance.soulcrush );
    debuffs.t29_vengeance_4pc =
        make_buff(
            *this, "decrepit_souls",
            p.set_bonuses.t29_vengeance_4pc->ok() ? p.set_bonuses.t29_vengeance_4pc_debuff : spell_data_t::not_found() )
            ->set_default_value_from_effect( 1 )
            ->set_refresh_behavior( buff_refresh_behavior::DURATION );
  }

  // TODO: make this conditional on hero spec
  debuffs.reavers_mark =
      make_buff( *this, "reavers_mark", p.hero_spec.reavers_mark )->set_default_value_from_effect( 1 );

  dots.sigil_of_flame = target->get_dot( "sigil_of_flame", &p );
  dots.the_hunt       = target->get_dot( "the_hunt_dot", &p );

  debuffs.sigil_of_flame =
      make_buff( *this, "sigil_of_flame", p.spell.sigil_of_flame_damage )
          ->set_refresh_behavior( buff_refresh_behavior::DURATION )
          ->set_stack_behavior( p.talent.vengeance.ascending_flame->ok() ? buff_stack_behavior::ASYNCHRONOUS
                                                                         : buff_stack_behavior::DEFAULT )
          ->apply_affecting_aura( p.talent.vengeance.ascending_flame )
          ->apply_affecting_aura( p.talent.vengeance.chains_of_anger );

  debuffs.serrated_glaive = make_buff( *this, "serrated_glaive", p.spec.serrated_glaive_debuff )
                                ->set_refresh_behavior( buff_refresh_behavior::PANDEMIC )
                                ->set_default_value( p.talent.havoc.serrated_glaive->effectN( 1 ).percent() );
}

void demon_hunter_td_t::trigger_burning_blades( action_state_t* state )
{
  demon_hunter_t* p = static_cast<demon_hunter_t*>( source );

  if ( !p->talent.felscarred.burning_blades->ok() )
    return;

  if ( !action_t::result_is_hit( state->result ) )
    return;

  const double dot_damage = state->result_amount * p->talent.felscarred.burning_blades->effectN( 1 ).percent();
  residual_action::trigger( p->active.burning_blades, state->target, dot_damage );
}

// ==========================================================================
// Demon Hunter Definitions
// ==========================================================================

demon_hunter_t::demon_hunter_t( sim_t* sim, util::string_view name, race_e r )
  : parse_player_effects_t( sim, DEMON_HUNTER, name, r ),
    next_fragment_spawn( 0 ),
    soul_fragments(),
    frailty_accumulator( 0.0 ),
    frailty_driver( nullptr ),
    fodder_initiative( false ),
    shattered_destiny_accumulator( 0.0 ),
    exit_melee_event( nullptr ),
    buff(),
    talent(),
    spec(),
    mastery(),
    cooldown(),
    gain(),
    benefits(),
    proc(),
    active(),
    options()
{
  create_cooldowns();
  create_gains();
  create_benefits();

  resource_regeneration = regen_type::DISABLED;
}

// ==========================================================================
// overridden player_t init functions
// ==========================================================================

stat_e demon_hunter_t::convert_hybrid_stat( stat_e s ) const
{
  // this converts hybrid stats that either morph based on spec or only work
  // for certain specs into the appropriate "basic" stats
  switch ( s )
  {
    case STAT_STR_AGI_INT:
    case STAT_AGI_INT:
    case STAT_STR_AGI:
      return STAT_AGILITY;
    case STAT_STR_INT:
      return STAT_NONE;
    case STAT_SPIRIT:
      return STAT_NONE;
    case STAT_BONUS_ARMOR:
      return specialization() == DEMON_HUNTER_VENGEANCE ? s : STAT_NONE;
    default:
      return s;
  }
}

void demon_hunter_t::copy_from( player_t* source )
{
  base_t::copy_from( source );

  auto source_p = debug_cast<demon_hunter_t*>( source );

  options = source_p->options;
}

action_t* demon_hunter_t::create_action( util::string_view name, util::string_view options_str )
{
  using namespace actions::heals;

  if ( name == "soul_barrier" )
    return new soul_barrier_t( this, options_str );

  using namespace actions::spells;

  if ( name == "blur" )
    return new blur_t( this, options_str );
  if ( name == "bulk_extraction" )
    return new bulk_extraction_t( this, options_str );
  if ( name == "chaos_nova" )
    return new chaos_nova_t( this, options_str );
  if ( name == "consume_magic" )
    return new consume_magic_t( this, options_str );
  if ( name == "demon_spikes" )
    return new demon_spikes_t( this, options_str );
  if ( name == "disrupt" )
    return new disrupt_t( this, options_str );
  if ( name == "eye_beam" )
    return new eye_beam_t( this, options_str );
  if ( name == "fel_barrage" )
    return new fel_barrage_t( this, options_str );
  if ( name == "fel_eruption" )
    return new fel_eruption_t( this, options_str );
  if ( name == "fel_devastation" )
    return new fel_devastation_t( this, options_str );
  if ( name == "fiery_brand" )
    return new fiery_brand_t( "fiery_brand", this, options_str );
  if ( name == "glaive_tempest" )
    return new glaive_tempest_t( this, options_str );
  if ( name == "infernal_strike" )
    return new infernal_strike_t( this, options_str );
  if ( name == "immolation_aura" )
    return new immolation_aura_t( this, options_str );
  if ( name == "metamorphosis" )
    return new metamorphosis_t( this, options_str );
  if ( name == "pick_up_fragment" )
    return new pick_up_fragment_t( this, options_str );
  if ( name == "sigil_of_flame" )
    return new sigil_of_flame_t( this, options_str );
  if ( name == "spirit_bomb" )
    return new spirit_bomb_t( this, options_str );
  if ( name == "spirit_burst" )
    return new spirit_burst_t( this, options_str );
  if ( name == "sigil_of_spite" )
    return new sigil_of_spite_t( this, options_str );
  if ( name == "the_hunt" )
    return new the_hunt_t( this, options_str );
  if ( name == "spectral_sight" )
    return new spectral_sight_t( this, options_str );
  if ( name == "sigil_of_misery" )
    return new sigil_of_misery_t( this, options_str );
  if ( name == "sigil_of_silence" )
    return new sigil_of_silence_t( this, options_str );
  if ( name == "sigil_of_chains" )
    return new sigil_of_chains_t( this, options_str );

  using namespace actions::attacks;

  if ( name == "auto_attack" )
    return new auto_attack_t( this, options_str );
  if ( name == "annihilation" )
    return new annihilation_t( "annihilation", this, options_str );
  if ( name == "blade_dance" )
    return new blade_dance_t( this, options_str );
  if ( name == "chaos_strike" )
    return new chaos_strike_t( "chaos_strike", this, options_str );
  if ( name == "essence_break" )
    return new essence_break_t( this, options_str );
  if ( name == "death_sweep" )
    return new death_sweep_t( this, options_str );
  if ( name == "demons_bite" )
    return new demons_bite_t( this, options_str );
  if ( name == "felblade" )
    return new felblade_t( this, options_str );
  if ( name == "fel_rush" )
    return new fel_rush_t( this, options_str );
  if ( name == "fracture" )
    return new fracture_t( this, options_str );
  if ( name == "shear" )
    return new shear_t( this, options_str );
  if ( name == "soul_cleave" )
    return new soul_cleave_t( this, options_str );
  if ( name == "soul_sunder" )
    return new soul_sunder_t( this, options_str );
  if ( name == "throw_glaive" )
    return new throw_glaive_t( "throw_glaive", this, options_str );
  if ( name == "vengeful_retreat" )
    return new vengeful_retreat_t( this, options_str );
  if ( name == "soul_carver" )
    return new soul_carver_t( this, options_str );
  if ( name == "reavers_glaive" )
    return new reavers_glaive_t( this, options_str );

  return base_t::create_action( name, options_str );
}

void demon_hunter_t::create_buffs()
{
  base_t::create_buffs();

  using namespace buffs;

  // General ================================================================

  buff.demon_soul           = make_buff( this, "demon_soul", spell.demon_soul );
  buff.empowered_demon_soul = make_buff( this, "empowered_demon_soul", spell.demon_soul_empowered );
  buff.fodder_to_the_flame = make_buff( this, "fodder_to_the_flame", spell.fodder_to_the_flame->effectN( 1 ).trigger() )
                                 ->set_cooldown( 0_s )
                                 ->set_chance( 1.0 )
                                 ->set_stack_change_callback( [ this ]( buff_t*, int o, int n ) {
                                   if ( n < o )
                                   {
                                     active.fodder_to_the_flame_damage->execute();
                                   }
                                   else
                                   {
                                     fodder_initiative = true;
                                   }
                                 } )
                                 ->set_period( 0.5_s )
                                 ->set_tick_callback( [ this ]( buff_t*, int t, timespan_t ) {
                                   if ( t < 3 )
                                     return;

                                   if ( rng().roll( options.fodder_to_the_flame_initiative_chance * ( t - 3 ) ) )
                                     fodder_initiative = false;
                                 } );
  buff.immolation_aura = make_buff<buffs::immolation_aura_buff_t>( this );
  buff.metamorphosis   = make_buff<buffs::metamorphosis_buff_t>( this );

  // Havoc ==================================================================

  buff.out_of_range = make_buff( this, "out_of_range", spell_data_t::nil() )->set_chance( 1.0 );

  buff.fel_rush_move = new buffs::movement_buff_t( this, "fel_rush_movement", spell_data_t::nil() );
  buff.fel_rush_move->set_chance( 1.0 )->set_duration( spec.fel_rush->gcd() );

  buff.vengeful_retreat_move = new buffs::movement_buff_t( this, "vengeful_retreat_movement", spell_data_t::nil() );
  buff.vengeful_retreat_move->set_chance( 1.0 )->set_duration( talent.demon_hunter.vengeful_retreat->duration() );

  buff.metamorphosis_move = new buffs::movement_buff_t( this, "metamorphosis_movement", spell_data_t::nil() );
  buff.metamorphosis_move->set_chance( 1.0 )->set_duration( 1_s );

  buff.blind_fury = make_buff( this, "blind_fury", talent.havoc.eye_beam )
                        ->set_default_value( talent.havoc.blind_fury->effectN( 3 ).resource( RESOURCE_FURY ) / 50 )
                        ->set_cooldown( timespan_t::zero() )
                        ->set_period( timespan_t::from_millis( 100 ) )  // Fake natural regeneration rate
                        ->set_tick_on_application( false )
                        ->set_tick_callback( [ this ]( buff_t* b, int, timespan_t ) {
                          resource_gain( RESOURCE_FURY, b->check_value(), gain.blind_fury );
                        } );

  buff.blur = make_buff( this, "blur", spec.blur->effectN( 1 ).trigger() )
                  ->set_default_value_from_effect_type( A_MOD_DAMAGE_PERCENT_TAKEN )
                  ->set_cooldown( timespan_t::zero() )
                  ->add_invalidate( CACHE_LEECH )
                  ->add_invalidate( CACHE_DODGE );

  buff.furious_gaze = make_buff( this, "furious_gaze", spec.furious_gaze_buff )
                          ->set_refresh_behavior( buff_refresh_behavior::DURATION )
                          ->set_default_value_from_effect_type( A_HASTE_ALL )
                          ->set_pct_buff_type( STAT_PCT_BUFF_HASTE );

  buff.initiative = make_buff( this, "initiative", spec.initiative_buff )
                        ->set_default_value_from_effect_type( A_MOD_ALL_CRIT_CHANCE )
                        ->set_pct_buff_type( STAT_PCT_BUFF_CRIT );

  buff.momentum = make_buff( this, "momentum", spec.momentum_buff );
  buff.momentum->set_refresh_duration_callback( []( const buff_t* b, timespan_t d ) {
    return std::min( b->remains() + d, 30_s );  // Capped to 30 seconds
  } );

  buff.inertia = make_buff( this, "inertia", spec.inertia_buff );
  buff.inertia->set_refresh_duration_callback( []( const buff_t* b, timespan_t d ) {
    return std::min( b->remains() + d, 10_s );  // Capped to 10 seconds
  } );

  buff.inner_demon = make_buff( this, "inner_demon", spec.inner_demon_buff );

  buff.restless_hunter = make_buff( this, "restless_hunter", spec.restless_hunter_buff );

  buff.tactical_retreat = make_buff( this, "tactical_retreat", spec.tactical_retreat_buff )
                              ->set_default_value_from_effect_type( A_PERIODIC_ENERGIZE )
                              ->set_tick_callback( [ this ]( buff_t* b, int, timespan_t ) {
                                resource_gain( RESOURCE_FURY, b->check_value(), gain.tactical_retreat );
                              } );

  buff.unbound_chaos = make_buff( this, "unbound_chaos", spec.unbound_chaos_buff )
                           ->set_default_value( talent.havoc.unbound_chaos->effectN( 2 ).percent() );

  buff.chaos_theory = make_buff( this, "chaos_theory", spec.chaos_theory_buff );

  buff.fel_barrage = new buffs::fel_barrage_buff_t( this );

  // Vengeance ==============================================================

  buff.demon_spikes = new buffs::demon_spikes_t( this );

  buff.calcified_spikes = new buffs::calcified_spikes_t( this );

  buff.painbringer = make_buff( this, "painbringer", spec.painbringer_buff )
                         ->set_default_value( talent.vengeance.painbringer->effectN( 1 ).percent() )
                         ->set_refresh_behavior( buff_refresh_behavior::DURATION )
                         ->set_stack_behavior( buff_stack_behavior::ASYNCHRONOUS )
                         ->set_period( 0_ms );

  buff.soul_furnace_damage_amp =
      make_buff( this, "soul_furnace_damage_amp", spec.soul_furnace_damage_amp )->set_default_value_from_effect( 1 );
  buff.soul_furnace_stack = make_buff( this, "soul_furnace_stack", spec.soul_furnace_stack );

  buff.soul_fragments = make_buff( this, "soul_fragments", spec.soul_fragments_buff )->set_max_stack( 10 );

  buff.soul_barrier = make_buff<absorb_buff_t>( this, "soul_barrier", talent.vengeance.soul_barrier );
  buff.soul_barrier->set_absorb_source( get_stats( "soul_barrier" ) )
      ->set_absorb_gain( get_gain( "soul_barrier" ) )
      ->set_absorb_high_priority( true )  // TOCHECK
      ->set_cooldown( timespan_t::zero() );

  // Aldrachi Reaver ========================================================

  buff.art_of_the_glaive = make_buff( this, "art_of_the_glaive", hero_spec.art_of_the_glaive_buff );
  buff.glaive_flurry     = make_buff( this, "glaive_flurry", hero_spec.glaive_flurry );
  buff.rending_strike    = make_buff( this, "rending_strike", hero_spec.rending_strike );
  buff.warblades_hunger  = make_buff( this, "warblades_hunger", hero_spec.warblades_hunger_buff );

  // Fel-scarred ============================================================

  buff.enduring_torment = make_buff( this, "enduring_torment", hero_spec.enduring_torment_buff )
                              ->set_constant_behavior( buff_constant_behavior::NEVER_CONSTANT )
                              ->set_allow_precombat( true );
  if ( specialization() == DEMON_HUNTER_HAVOC )
  {
    buff.enduring_torment->set_default_value_from_effect_type( A_HASTE_ALL )->set_pct_buff_type( STAT_PCT_BUFF_HASTE );
  }
  buff.monster_rising = make_buff( this, "monster_rising", hero_spec.monster_rising_buff )
                            ->set_default_value_from_effect_type( A_MOD_PERCENT_STAT )
                            ->set_pct_buff_type( STAT_PCT_BUFF_AGILITY )
                            ->set_allow_precombat( true )
                            ->set_constant_behavior( buff_constant_behavior::NEVER_CONSTANT );
  buff.pursuit_of_angryness =
      make_buff( this, "pursuit_of_angriness", talent.felscarred.pursuit_of_angriness )
          ->set_quiet( true )
          ->set_tick_zero( true )
          ->add_invalidate( CACHE_RUN_SPEED )
          ->set_tick_callback(
              [ this, speed_per_fury = talent.felscarred.pursuit_of_angriness->effectN( 1 ).percent() /
                                       talent.felscarred.pursuit_of_angriness->effectN( 1 ).base_value() ](
                  buff_t* b, int, timespan_t ) {
                // TOCHECK - Does this need to floor if it's not a whole number
                b->current_value = resources.current[ RESOURCE_FURY ] * speed_per_fury;
              } );
  buff.student_of_suffering =
      make_buff( this, "student_of_suffering", hero_spec.student_of_suffering_buff )
          ->set_default_value_from_effect_type( A_MOD_MASTERY_PCT )
          ->set_pct_buff_type( STAT_PCT_BUFF_MASTERY )
          ->set_tick_behavior( buff_tick_behavior::REFRESH )
          ->set_tick_on_application( false )
          ->set_period( 2_s )
          ->set_tick_callback( [ this ]( buff_t* b, int, timespan_t ) {
            resource_gain( RESOURCE_FURY, b->data().effectN( 2 ).trigger()->effectN( 1 ).base_value(),
                           gain.student_of_suffering );
          } );

  auto demonsurge_spec_abilities =
      specialization() == DEMON_HUNTER_HAVOC ? actions::demonsurge_havoc_abilities : actions::demonsurge_vengeance_abilities;
  for ( actions::demonsurge_ability ability : demonsurge_spec_abilities )
  {
    buff.demonsurge_abilities[ ability ] = make_buff( this, demonsurge_ability_name( ability ), spell_data_t::nil() );
  }

  buff.demonsurge_demonic  = make_buff( this, "demonsurge_demonic", hero_spec.demonsurge_demonic_buff );
  buff.demonsurge_hardcast = make_buff( this, "demonsurge_hardcast", hero_spec.demonsurge_hardcast_buff );
  buff.demonsurge          = make_buff( this, "demonsurge", hero_spec.demonsurge_stacking_buff );

  // Set Bonus Items ========================================================

  buff.t29_havoc_4pc = make_buff( this, "seething_chaos",
                                  set_bonuses.t29_havoc_4pc->ok() ? find_spell( 394934 ) : spell_data_t::not_found() )
                           ->set_refresh_behavior( buff_refresh_behavior::DURATION );

  buff.t30_havoc_2pc =
      make_buff( this, "seething_fury",
                 set_bonuses.t30_havoc_2pc->ok() ? set_bonuses.t30_havoc_2pc_buff : spell_data_t::not_found() )
          ->set_default_value_from_effect_type( A_MOD_TOTAL_STAT_PERCENTAGE )
          ->set_pct_buff_type( STAT_PCT_BUFF_AGILITY );

  buff.t30_havoc_4pc =
      make_buff( this, "seething_potential",
                 set_bonuses.t30_havoc_4pc->ok() ? set_bonuses.t30_havoc_4pc_buff : spell_data_t::not_found() )
          ->set_default_value_from_effect( 1 );

  buff.t30_vengeance_2pc =
      make_buff( this, "fires_of_fel",
                 set_bonuses.t30_vengeance_2pc->ok() ? set_bonuses.t30_vengeance_2pc_buff : spell_data_t::not_found() )
          ->set_default_value_from_effect( 1 )
          ->set_stack_behavior( buff_stack_behavior::ASYNCHRONOUS );
  buff.t30_vengeance_4pc =
      make_buff( this, "recrimination",
                 set_bonuses.t30_vengeance_4pc->ok() ? set_bonuses.t30_vengeance_4pc_buff : spell_data_t::not_found() );

  buff.t31_vengeance_2pc =
      make_buff( this, "fiery_resolve",
                 set_bonuses.t31_vengeance_2pc->ok() ? set_bonuses.t31_vengeance_2pc_buff : spell_data_t::not_found() )
          ->add_invalidate( CACHE_STAMINA );

  buff.tww1_havoc_4pc =
      make_buff( this, "blade_rhapsody",
                 set_bonuses.tww1_havoc_4pc->ok() ? set_bonuses.tww1_havoc_4pc_buff : spell_data_t::not_found() )
          ->set_default_value_from_effect_type( A_ADD_PCT_MODIFIER );

  buff.tww1_vengeance_4pc = make_buff( this, "soulfuse",
                                       set_bonuses.tww1_vengeance_4pc->ok() ? set_bonuses.tww1_vengeance_4pc_buff
                                                                            : spell_data_t::not_found() )
                                ->set_default_value_from_effect_type( A_ADD_PCT_MODIFIER, P_GENERIC );
}

std::unique_ptr<expr_t> demon_hunter_t::create_expression( util::string_view name_str )
{
  auto splits = util::string_split( name_str, "." );

  if ( ( splits.size() == 1 || splits.size() == 2 ) &&
       ( util::str_compare_ci( splits[ 0 ], "soul_fragments" ) ||
         util::str_compare_ci( splits[ 0 ], "greater_soul_fragments" ) ||
         util::str_compare_ci( splits[ 0 ], "lesser_soul_fragments" ) ||
         util::str_compare_ci( splits[ 0 ], "demon_soul_fragments" ) ) )
  {
    enum class soul_fragment_filter
    {
      ACTIVE,
      INACTIVE,
      TOTAL
    };

    struct soul_fragments_expr_t : public expr_t
    {
      demon_hunter_t* dh;
      soul_fragment type;
      soul_fragment_filter filter;

      soul_fragments_expr_t( demon_hunter_t* p, util::string_view n, soul_fragment t, soul_fragment_filter f )
        : expr_t( n ), dh( p ), type( t ), filter( f )
      {
      }

      double evaluate() override
      {
        switch ( filter )
        {
          case soul_fragment_filter::ACTIVE:
            return dh->get_active_soul_fragments( type );
          case soul_fragment_filter::INACTIVE:
            return dh->get_inactive_soul_fragments( type );
          case soul_fragment_filter::TOTAL:
            return dh->get_total_soul_fragments( type );
          default:
            return 0;
        }
      }
    };

    soul_fragment type = soul_fragment::LESSER;

    if ( util::str_compare_ci( splits[ 0 ], "soul_fragments" ) )
    {
      type = soul_fragment::ANY;
    }
    else if ( util::str_compare_ci( splits[ 0 ], "greater_soul_fragments" ) )
    {
      type = soul_fragment::ANY_GREATER;
    }
    else if ( util::str_compare_ci( splits[ 0 ], "demon_soul_fragments" ) )
    {
      type = soul_fragment::ANY_DEMON;
    }

    soul_fragment_filter filter = soul_fragment_filter::ACTIVE;

    if ( splits.size() == 2 )
    {
      if ( util::str_compare_ci( splits[ 1 ], "inactive" ) )
      {
        filter = soul_fragment_filter::INACTIVE;
      }
      else if ( util::str_compare_ci( splits[ 1 ], "total" ) )
      {
        filter = soul_fragment_filter::TOTAL;
      }
      else if ( !util::str_compare_ci( splits[ 1 ], "active" ) )
      {
        throw std::invalid_argument( fmt::format( "Unsupported soul_fragments filter '{}'.", splits[ 1 ] ) );
      }
    }

    return std::make_unique<soul_fragments_expr_t>( this, name_str, type, filter );
  }
  else if ( name_str == "cooldown.metamorphosis.adjusted_remains" )
  {
    return this->cooldown.metamorphosis->create_expression( "remains" );
  }
  else if ( name_str == "cooldown.eye_beam.adjusted_remains" )
  {
    if ( this->talent.havoc.cycle_of_hatred->ok() )
    {
      return std::make_unique<expressions::eye_beam_adjusted_cooldown_expr_t>( this, name_str );
    }
    else
    {
      return this->cooldown.eye_beam->create_expression( "remains" );
    }
  }
  else if ( util::str_compare_ci( name_str, "seething_fury_threshold" ) )
  {
    return expr_t::create_constant( "seething_fury_threshold",
                                    this->set_bonuses.t30_havoc_2pc->effectN( 1 ).base_value() );
  }
  else if ( util::str_compare_ci( name_str, "seething_fury_spent" ) )
  {
    return make_mem_fn_expr( "seething_fury_spent", this->set_bonuses,
                             &demon_hunter_t::set_bonuses_t::t30_havoc_2pc_fury_tracker );
  }
  else if ( util::str_compare_ci( name_str, "seething_fury_deficit" ) )
  {
    if ( this->set_bonuses.t30_havoc_2pc->ok() )
    {
      return make_fn_expr( "seething_fury_deficit", [ this ] {
        return this->set_bonuses.t30_havoc_2pc->effectN( 1 ).base_value() -
               this->set_bonuses.t30_havoc_2pc_fury_tracker;
      } );
    }
    else
    {
      return expr_t::create_constant( "seething_fury_deficit", 0.0 );
    }
  }

  return player_t::create_expression( name_str );
}

void demon_hunter_t::create_options()
{
  player_t::create_options();

  add_option( opt_float( "target_reach", options.target_reach ) );
  add_option( opt_float( "movement_direction_factor", options.movement_direction_factor, 1.0, 2.0 ) );
  add_option( opt_float( "initial_fury", options.initial_fury, 0.0, 120 ) );
  add_option(
      opt_float( "fodder_to_the_flame_initiative_chance", options.fodder_to_the_flame_initiative_chance, 0, 1 ) );
  add_option(
      opt_float( "soul_fragment_movement_consume_chance", options.soul_fragment_movement_consume_chance, 0, 1 ) );
}

std::string demon_hunter_t::create_profile( save_e type )
{
  std::string profile_str = base_t::create_profile( type );

  // Log all options here

  return profile_str;
}

void demon_hunter_t::init_absorb_priority()
{
  player_t::init_absorb_priority();

  absorb_priority.push_back( 227225 );  // Soul Barrier
}

void demon_hunter_t::init_action_list()
{
  if ( main_hand_weapon.type == WEAPON_NONE || off_hand_weapon.type == WEAPON_NONE )
  {
    if ( !quiet )
    {
      sim->errorf( "Player %s does not have a valid main-hand and off-hand weapon.", name() );
    }
    quiet = true;
    return;
  }

  if ( !action_list_str.empty() )
  {
    player_t::init_action_list();
    return;
  }
  clear_action_priority_lists();

  if ( specialization() == DEMON_HUNTER_HAVOC )
  {
    demon_hunter_apl::havoc( this );
  }
  else if ( specialization() == DEMON_HUNTER_VENGEANCE )
  {
    demon_hunter_apl::vengeance( this );
  }

  use_default_action_list = true;

  base_t::init_action_list();
}

void demon_hunter_t::init_base_stats()
{
  if ( base.distance < 1 )
    base.distance = 5.0;

  base_t::init_base_stats();

  resources.base[ RESOURCE_FURY ] = 100;
  resources.base[ RESOURCE_FURY ] += talent.demon_hunter.unrestrained_fury->effectN( 1 ).base_value();
  resources.base[ RESOURCE_FURY ] += talent.felscarred.untethered_fury->effectN( 1 ).base_value();

  base.attack_power_per_strength = 0.0;
  base.attack_power_per_agility  = 1.0;
  base.spell_power_per_intellect = 1.0;

  // Avoidance diminishing Returns constants/conversions now handled in
  // player_t::init_base_stats().
  // Base miss, dodge, parry, and block are set in player_t::init_base_stats().
  // Just need to add class- or spec-based modifiers here.

  base_gcd = timespan_t::from_seconds( 1.5 );
}

void demon_hunter_t::init_procs()
{
  base_t::init_procs();

  // General
  proc.delayed_aa_range              = get_proc( "delayed_aa_out_of_range" );
  proc.soul_fragment_greater         = get_proc( "soul_fragment_greater" );
  proc.soul_fragment_greater_demon   = get_proc( "soul_fragment_greater_demon" );
  proc.soul_fragment_empowered_demon = get_proc( "soul_fragment_empowered_demon" );
  proc.soul_fragment_lesser          = get_proc( "soul_fragment_lesser" );
  proc.felblade_reset                = get_proc( "felblade_reset" );

  // Havoc
  proc.demonic_appetite                = get_proc( "demonic_appetite" );
  proc.demons_bite_in_meta             = get_proc( "demons_bite_in_meta" );
  proc.chaos_strike_in_essence_break   = get_proc( "chaos_strike_in_essence_break" );
  proc.annihilation_in_essence_break   = get_proc( "annihilation_in_essence_break" );
  proc.blade_dance_in_essence_break    = get_proc( "blade_dance_in_essence_break" );
  proc.death_sweep_in_essence_break    = get_proc( "death_sweep_in_essence_break" );
  proc.chaos_strike_in_serrated_glaive = get_proc( "chaos_strike_in_serrated_glaive" );
  proc.annihilation_in_serrated_glaive = get_proc( "annihilation_in_serrated_glaive" );
  proc.throw_glaive_in_serrated_glaive = get_proc( "throw_glaive_in_serrated_glaive" );
  proc.shattered_destiny               = get_proc( "shattered_destiny" );
  proc.eye_beam_canceled               = get_proc( "eye_beam_canceled" );

  // Vengeance
  proc.soul_fragment_expire              = get_proc( "soul_fragment_expire" );
  proc.soul_fragment_overflow            = get_proc( "soul_fragment_overflow" );
  proc.soul_fragment_from_shear          = get_proc( "soul_fragment_from_shear" );
  proc.soul_fragment_from_fracture       = get_proc( "soul_fragment_from_fracture" );
  proc.soul_fragment_from_sigil_of_spite = get_proc( "soul_fragment_from_sigil_of_spite" );
  proc.soul_fragment_from_fallout        = get_proc( "soul_fragment_from_fallout" );
  proc.soul_fragment_from_meta           = get_proc( "soul_fragment_from_meta" );

  // Set Bonuses
  proc.soul_fragment_from_t29_2pc   = get_proc( "soul_fragment_from_t29_2pc" );
  proc.soul_fragment_from_t31_4pc   = get_proc( "soul_fragment_from_t31_4pc" );
  proc.soul_fragment_from_twws1_2pc = get_proc( "soul_fragment_from_twws1_2pc" );
}

void demon_hunter_t::init_uptimes()
{
  base_t::init_uptimes();
}

void demon_hunter_t::init_resources( bool force )
{
  base_t::init_resources( force );

  resources.current[ RESOURCE_FURY ] = options.initial_fury;
  expected_max_health                = calculate_expected_max_health();
}

void demon_hunter_t::init_special_effects()
{
  base_t::init_special_effects();
}

void demon_hunter_t::init_rng()
{
  // RPPM objects

  // General
  if ( specialization() == DEMON_HUNTER_HAVOC )
  {
    rppm.felblade         = get_rppm( "felblade", spell.felblade_reset_havoc );
    rppm.demonic_appetite = get_rppm( "demonic_appetite", spec.demonic_appetite );
  }
  else  // DEMON_HUNTER_VENGEANCE
  {
    rppm.felblade = get_rppm( "felblade", spell.felblade_reset_vengeance );
  }

  player_t::init_rng();
}

void demon_hunter_t::init_scaling()
{
  base_t::init_scaling();

  scaling->enable( STAT_WEAPON_OFFHAND_DPS );

  if ( specialization() == DEMON_HUNTER_VENGEANCE )
    scaling->enable( STAT_BONUS_ARMOR );

  scaling->disable( STAT_STRENGTH );
}

void demon_hunter_t::init_spells()
{
  base_t::init_spells();

  // Specialization =========================================================

  // General Passives
  spell.all_demon_hunter       = dbc::get_class_passive( *this, SPEC_NONE );
  spell.chaos_brand            = find_spell( 1490 );
  spell.critical_strikes       = find_spell( 221351 );
  spell.leather_specialization = find_specialization_spell( "Leather Specialization" );

  spell.demon_soul           = find_spell( 163073 );
  spell.demon_soul_empowered = find_spell( 347765 );
  spell.soul_fragment        = find_spell( 204255 );

  // Shared Abilities
  spell.disrupt           = find_class_spell( "Disrupt" );
  spell.immolation_aura   = find_class_spell( "Immolation Aura" );
  spell.immolation_aura_2 = find_rank_spell( "Immolation Aura", "Rank 2" );
  spell.spectral_sight    = find_class_spell( "Spectral Sight" );

  // Spec-Overriden Passives
  spec.demonic_wards       = find_specialization_spell( "Demonic Wards" );
  spec.demonic_wards_2     = find_rank_spell( "Demonic Wards", "Rank 2" );
  spec.demonic_wards_3     = find_rank_spell( "Demonic Wards", "Rank 3" );
  spec.immolation_aura_cdr = find_spell( 320378, DEMON_HUNTER_VENGEANCE );
  spec.thick_skin          = find_specialization_spell( "Thick Skin" );

  if ( specialization() == DEMON_HUNTER_HAVOC )
  {
    spell.throw_glaive        = find_class_spell( "Throw Glaive" );
    spec.consume_soul_greater = find_spell( 178963 );
    spec.consume_soul_lesser  = spec.consume_soul_greater;
    spec.metamorphosis        = find_class_spell( "Metamorphosis" );
    spec.metamorphosis_buff   = spec.metamorphosis->effectN( 2 ).trigger();
  }
  else
  {
    spell.throw_glaive        = find_specialization_spell( "Throw Glaive" );
    spec.consume_soul_greater = find_spell( 210042 );
    spec.consume_soul_lesser  = find_spell( 203794 );
    spec.metamorphosis        = find_specialization_spell( "Metamorphosis" );
    spec.metamorphosis_buff   = spec.metamorphosis;
  }

  // Havoc Spells
  spec.havoc_demon_hunter = find_specialization_spell( "Havoc Demon Hunter" );

  spec.annihilation          = find_spell( 201427, DEMON_HUNTER_HAVOC );
  spec.blade_dance           = find_specialization_spell( "Blade Dance" );
  spec.blade_dance_2         = find_rank_spell( "Blade Dance", "Rank 2" );
  spec.blur                  = find_specialization_spell( "Blur" );
  spec.chaos_strike          = find_specialization_spell( "Chaos Strike" );
  spec.chaos_strike_fury     = find_spell( 193840, DEMON_HUNTER_HAVOC );
  spec.chaos_strike_refund   = find_spell( 197125, DEMON_HUNTER_HAVOC );
  spec.death_sweep           = find_spell( 210152, DEMON_HUNTER_HAVOC );
  spec.demons_bite           = find_spell( 162243, DEMON_HUNTER_HAVOC );
  spec.fel_rush              = find_specialization_spell( "Fel Rush" );
  spec.fel_rush_damage       = find_spell( 192611, DEMON_HUNTER_HAVOC );
  spec.immolation_aura_3     = find_rank_spell( "Immolation Aura", "Rank 3" );
  spec.fel_eruption          = find_specialization_spell( "Fel Eruption" );
  spec.demonic_appetite      = find_spell( 206478, DEMON_HUNTER_HAVOC );
  spec.demonic_appetite_fury = find_spell( 210041, DEMON_HUNTER_HAVOC );

  // Vengeance Spells
  spec.vengeance_demon_hunter = find_specialization_spell( "Vengeance Demon Hunter" );

  spec.demon_spikes        = find_specialization_spell( "Demon Spikes" );
  spec.infernal_strike     = find_specialization_spell( "Infernal Strike" );
  spec.soul_cleave         = find_specialization_spell( "Soul Cleave" );
  spec.shear               = find_specialization_spell( "Shear" );
  spec.soul_cleave_2       = find_rank_spell( "Soul Cleave", "Rank 2" );
  spec.riposte             = find_specialization_spell( "Riposte" );
  spec.soul_fragments_buff = find_spell( 203981, DEMON_HUNTER_VENGEANCE );

  // Masteries ==============================================================

  mastery.demonic_presence = find_mastery_spell( DEMON_HUNTER_HAVOC );
  mastery.fel_blood        = find_mastery_spell( DEMON_HUNTER_VENGEANCE );
  mastery.fel_blood_rank_2 = find_rank_spell( "Mastery: Fel Blood", "Rank 2" );

  // Talents ================================================================

  talent.demon_hunter.vengeful_retreat = find_talent_spell( talent_tree::CLASS, "Vengeful Retreat" );
  talent.demon_hunter.blazing_path     = find_talent_spell( talent_tree::CLASS, "Blazing Path" );
  talent.demon_hunter.sigil_of_misery  = find_talent_spell( talent_tree::CLASS, "Sigil of Misery" );

  talent.demon_hunter.unrestrained_fury     = find_talent_spell( talent_tree::CLASS, "Unrestrained Fury" );
  talent.demon_hunter.imprison              = find_talent_spell( talent_tree::CLASS, "Imprison" );
  talent.demon_hunter.shattered_restoration = find_talent_spell( talent_tree::CLASS, "Shattered Restoration" );

  talent.demon_hunter.vengeful_bonds           = find_talent_spell( talent_tree::CLASS, "Vengeful Bonds" );
  talent.demon_hunter.improved_disrupt         = find_talent_spell( talent_tree::CLASS, "Improved Disrupt" );
  talent.demon_hunter.bouncing_glaives         = find_talent_spell( talent_tree::CLASS, "Bouncing Glaives" );
  talent.demon_hunter.consume_magic            = find_talent_spell( talent_tree::CLASS, "Consume Magic" );
  talent.demon_hunter.improved_sigil_of_misery = find_talent_spell( talent_tree::CLASS, "Improved Sigil of Misery" );

  talent.demon_hunter.pursuit           = find_talent_spell( talent_tree::CLASS, "Pursuit" );
  talent.demon_hunter.disrupting_fury   = find_talent_spell( talent_tree::CLASS, "Disrupting Fury" );
  talent.demon_hunter.felblade          = find_talent_spell( talent_tree::CLASS, "Felblade" );
  talent.demon_hunter.swallowed_anger   = find_talent_spell( talent_tree::CLASS, "Swallowed Anger" );
  talent.demon_hunter.charred_warblades = find_talent_spell( talent_tree::CLASS, "Charred Warblades" );

  talent.demon_hunter.felfire_haste          = find_talent_spell( talent_tree::CLASS, "Felfire Haste" );
  talent.demon_hunter.master_of_the_glaive   = find_talent_spell( talent_tree::CLASS, "Master of the Glaive" );
  talent.demon_hunter.champion_of_the_glaive = find_talent_spell( talent_tree::CLASS, "Champion of the Glaive" );
  talent.demon_hunter.aura_of_pain           = find_talent_spell( talent_tree::CLASS, "Aura of Pain" );
  talent.demon_hunter.precise_sigils         = find_talent_spell( talent_tree::CLASS, "Precise Sigils" );
  talent.demon_hunter.lost_in_darkness       = find_talent_spell( talent_tree::CLASS, "Lost in Darkness" );

  talent.demon_hunter.chaos_nova      = find_talent_spell( talent_tree::CLASS, "Chaos Nova" );
  talent.demon_hunter.soul_rending    = find_talent_spell( talent_tree::CLASS, "Soul Rending" );
  talent.demon_hunter.infernal_armor  = find_talent_spell( talent_tree::CLASS, "Infernal Armor" );
  talent.demon_hunter.aldrachi_design = find_talent_spell( talent_tree::CLASS, "Aldrachi Design" );

  talent.demon_hunter.chaos_fragments      = find_talent_spell( talent_tree::CLASS, "Chaos Fragments" );
  talent.demon_hunter.illidari_knowledge   = find_talent_spell( talent_tree::CLASS, "Illidari Knowledge" );
  talent.demon_hunter.demonic              = find_talent_spell( talent_tree::CLASS, "Demonic" );
  talent.demon_hunter.will_of_the_illidari = find_talent_spell( talent_tree::CLASS, "Will of the Illidari" );
  talent.demon_hunter.live_by_the_glaive   = find_talent_spell( talent_tree::CLASS, "Live by the Glaive" );

  talent.demon_hunter.internal_struggle = find_talent_spell( talent_tree::CLASS, "Internal Struggle" );
  talent.demon_hunter.darkness          = find_talent_spell( talent_tree::CLASS, "Darkness" );
  talent.demon_hunter.soul_sigils       = find_talent_spell( talent_tree::CLASS, "Soul Sigils" );
  talent.demon_hunter.quickened_sigils  = find_talent_spell( talent_tree::CLASS, "Quickened Sigils" );

  talent.demon_hunter.erratic_felheart = find_talent_spell( talent_tree::CLASS, "Erratic Felheart" );
  talent.demon_hunter.long_night       = find_talent_spell( talent_tree::CLASS, "Long Night" );
  talent.demon_hunter.pitch_black      = find_talent_spell( talent_tree::CLASS, "Pitch Black" );
  talent.demon_hunter.rush_of_chaos    = find_talent_spell( talent_tree::CLASS, "Rush of Chaos" );
  talent.demon_hunter.demon_muzzle     = find_talent_spell( talent_tree::CLASS, "Demon Muzzle" );
  talent.demon_hunter.flames_of_fury   = find_talent_spell( talent_tree::CLASS, "Flames of Fury" );

  talent.demon_hunter.collective_anguish  = find_talent_spell( talent_tree::CLASS, "Collective Anguish" );
  talent.demon_hunter.fodder_to_the_flame = find_talent_spell( talent_tree::CLASS, "Fodder to the Flame" );
  talent.demon_hunter.the_hunt            = find_talent_spell( talent_tree::CLASS, "The Hunt" );
  talent.demon_hunter.sigil_of_spite      = find_talent_spell( talent_tree::CLASS, "Sigil of Spite" );

  // Havoc Talents

  talent.havoc.eye_beam = find_talent_spell( talent_tree::SPECIALIZATION, "Eye Beam" );

  talent.havoc.critical_chaos    = find_talent_spell( talent_tree::SPECIALIZATION, "Critical Chaos" );
  talent.havoc.insatiable_hunger = find_talent_spell( talent_tree::SPECIALIZATION, "Insatiable Hunger" );
  talent.havoc.demon_blades      = find_talent_spell( talent_tree::SPECIALIZATION, "Demon Blades" );
  talent.havoc.burning_hatred    = find_talent_spell( talent_tree::SPECIALIZATION, "Burning Hatred" );

  talent.havoc.improved_fel_rush     = find_talent_spell( talent_tree::SPECIALIZATION, "Improved Fel Rush" );
  talent.havoc.dash_of_chaos         = find_talent_spell( talent_tree::SPECIALIZATION, "Dash of Chaos" );
  talent.havoc.improved_chaos_strike = find_talent_spell( talent_tree::SPECIALIZATION, "Improved Chaos Strike" );
  talent.havoc.first_blood           = find_talent_spell( talent_tree::SPECIALIZATION, "First Blood" );
  talent.havoc.accelerated_blade     = find_talent_spell( talent_tree::SPECIALIZATION, "Accelerated Blade" );
  talent.havoc.demon_hide            = find_talent_spell( talent_tree::SPECIALIZATION, "Demon Hide" );

  talent.havoc.desperate_instincts = find_talent_spell( talent_tree::SPECIALIZATION, "Desperate Instincts" );
  talent.havoc.netherwalk          = find_talent_spell( talent_tree::SPECIALIZATION, "Netherwalk" );
  talent.havoc.deflecting_dance    = find_talent_spell( talent_tree::SPECIALIZATION, "Deflecting Dance" );
  talent.havoc.mortal_dance        = find_talent_spell( talent_tree::SPECIALIZATION, "Mortal Dance" );

  talent.havoc.initiative             = find_talent_spell( talent_tree::SPECIALIZATION, "Initiative" );
  talent.havoc.scars_of_suffering     = find_talent_spell( talent_tree::SPECIALIZATION, "Scars of Suffering" );
  talent.havoc.chaotic_transformation = find_talent_spell( talent_tree::SPECIALIZATION, "Chaotic Transformation" );
  talent.havoc.furious_throws         = find_talent_spell( talent_tree::SPECIALIZATION, "Furious Throws" );
  talent.havoc.trail_of_ruin          = find_talent_spell( talent_tree::SPECIALIZATION, "Trail of Ruin" );

  talent.havoc.unbound_chaos     = find_talent_spell( talent_tree::SPECIALIZATION, "Unbound Chaos" );
  talent.havoc.blind_fury        = find_talent_spell( talent_tree::SPECIALIZATION, "Blind Fury" );
  talent.havoc.looks_can_kill    = find_talent_spell( talent_tree::SPECIALIZATION, "Looks Can Kill" );
  talent.havoc.dancing_with_fate = find_talent_spell( talent_tree::SPECIALIZATION, "Dancing with Fate" );
  talent.havoc.growing_inferno   = find_talent_spell( talent_tree::SPECIALIZATION, "Growing Inferno" );

  talent.havoc.tactical_retreat     = find_talent_spell( talent_tree::SPECIALIZATION, "Tactical Retreat" );
  talent.havoc.isolated_prey        = find_talent_spell( talent_tree::SPECIALIZATION, "Isolated Prey" );
  talent.havoc.furious_gaze         = find_talent_spell( talent_tree::SPECIALIZATION, "Furious Gaze" );
  talent.havoc.relentless_onslaught = find_talent_spell( talent_tree::SPECIALIZATION, "Relentless Onslaught" );
  talent.havoc.burning_wound        = find_talent_spell( talent_tree::SPECIALIZATION, "Burning Wound" );

  talent.havoc.momentum        = find_talent_spell( talent_tree::SPECIALIZATION, "Momentum" );
  talent.havoc.inertia         = find_talent_spell( talent_tree::SPECIALIZATION, "Inertia" );
  talent.havoc.chaos_theory    = find_talent_spell( talent_tree::SPECIALIZATION, "Chaos Theory" );
  talent.havoc.restless_hunter = find_talent_spell( talent_tree::SPECIALIZATION, "Restless Hunter" );
  talent.havoc.inner_demon     = find_talent_spell( talent_tree::SPECIALIZATION, "Inner Demon" );
  talent.havoc.serrated_glaive = find_talent_spell( talent_tree::SPECIALIZATION, "Serrated Glaive" );
  talent.havoc.ragefire        = find_talent_spell( talent_tree::SPECIALIZATION, "Ragefire" );

  talent.havoc.know_your_enemy     = find_talent_spell( talent_tree::SPECIALIZATION, "Know Your Enemy" );
  talent.havoc.glaive_tempest      = find_talent_spell( talent_tree::SPECIALIZATION, "Glaive Tempest" );
  talent.havoc.cycle_of_hatred     = find_talent_spell( talent_tree::SPECIALIZATION, "Cycle of Hatred" );
  talent.havoc.soulscar            = find_talent_spell( talent_tree::SPECIALIZATION, "Soulscar" );
  talent.havoc.chaotic_disposition = find_talent_spell( talent_tree::SPECIALIZATION, "Chaotic Disposition" );

  talent.havoc.essence_break       = find_talent_spell( talent_tree::SPECIALIZATION, "Essence Break" );
  talent.havoc.fel_barrage         = find_talent_spell( talent_tree::SPECIALIZATION, "Fel Barrage" );
  talent.havoc.shattered_destiny   = find_talent_spell( talent_tree::SPECIALIZATION, "Shattered Destiny" );
  talent.havoc.any_means_necessary = find_talent_spell( talent_tree::SPECIALIZATION, "Any Means Necessary" );
  talent.havoc.a_fire_inside       = find_talent_spell( talent_tree::SPECIALIZATION, "A Fire Inside" );

  // Vengeance Talents

  talent.vengeance.fel_devastation = find_talent_spell( talent_tree::SPECIALIZATION, "Fel Devastation" );

  talent.vengeance.frailty     = find_talent_spell( talent_tree::SPECIALIZATION, "Frailty" );
  talent.vengeance.fiery_brand = find_talent_spell( talent_tree::SPECIALIZATION, "Fiery Brand" );

  talent.vengeance.perfectly_balanced_glaive =
      find_talent_spell( talent_tree::SPECIALIZATION, "Perfectly Balanced Glaive" );
  talent.vengeance.deflecting_spikes = find_talent_spell( talent_tree::SPECIALIZATION, "Deflecting Spikes" );
  talent.vengeance.ascending_flame   = find_talent_spell( talent_tree::SPECIALIZATION, "Ascending Flame" );

  talent.vengeance.shear_fury       = find_talent_spell( talent_tree::SPECIALIZATION, "Shear Fury" );
  talent.vengeance.fracture         = find_talent_spell( talent_tree::SPECIALIZATION, "Fracture" );
  talent.vengeance.calcified_spikes = find_talent_spell( talent_tree::SPECIALIZATION, "Calcified Spikes" );
  talent.vengeance.roaring_fire     = find_talent_spell( talent_tree::SPECIALIZATION, "Roaring Fire" );
  talent.vengeance.sigil_of_silence = find_talent_spell( talent_tree::SPECIALIZATION, "Sigil of Silence" );
  talent.vengeance.retaliation      = find_talent_spell( talent_tree::SPECIALIZATION, "Retaliation" );
  talent.vengeance.meteoric_strikes = find_talent_spell( talent_tree::SPECIALIZATION, "Meteoric Strikes" );

  talent.vengeance.spirit_bomb      = find_talent_spell( talent_tree::SPECIALIZATION, "Spirit Bomb" );
  talent.vengeance.feast_of_souls   = find_talent_spell( talent_tree::SPECIALIZATION, "Feast of Souls" );
  talent.vengeance.agonizing_flames = find_talent_spell( talent_tree::SPECIALIZATION, "Agonizing Flames" );
  talent.vengeance.extended_spikes  = find_talent_spell( talent_tree::SPECIALIZATION, "Extended Spikes" );
  talent.vengeance.burning_blood    = find_talent_spell( talent_tree::SPECIALIZATION, "Burning Blood" );
  talent.vengeance.soul_barrier     = find_talent_spell( talent_tree::SPECIALIZATION, "Soul Barrier" );
  talent.vengeance.bulk_extraction  = find_talent_spell( talent_tree::SPECIALIZATION, "Bulk Extraction" );
  talent.vengeance.revel_in_pain    = find_talent_spell( talent_tree::SPECIALIZATION, "Revel in Pain" );

  talent.vengeance.void_reaver         = find_talent_spell( talent_tree::SPECIALIZATION, "Void Reaver" );
  talent.vengeance.fallout             = find_talent_spell( talent_tree::SPECIALIZATION, "Fallout" );
  talent.vengeance.ruinous_bulwark     = find_talent_spell( talent_tree::SPECIALIZATION, "Ruinous Bulwark" );
  talent.vengeance.volatile_flameblood = find_talent_spell( talent_tree::SPECIALIZATION, "Volatile Flameblood" );
  talent.vengeance.fel_flame_fortification =
      find_talent_spell( talent_tree::SPECIALIZATION, "Fel Flame Fortification" );

  talent.vengeance.soul_furnace    = find_talent_spell( talent_tree::SPECIALIZATION, "Soul Furnace" );
  talent.vengeance.painbringer     = find_talent_spell( talent_tree::SPECIALIZATION, "Painbringer" );
  talent.vengeance.sigil_of_chains = find_talent_spell( talent_tree::SPECIALIZATION, "Sigil of Chains" );
  talent.vengeance.fiery_demise    = find_talent_spell( talent_tree::SPECIALIZATION, "Fiery Demise" );
  talent.vengeance.chains_of_anger = find_talent_spell( talent_tree::SPECIALIZATION, "Chains of Anger" );

  talent.vengeance.focused_cleave   = find_talent_spell( talent_tree::SPECIALIZATION, "Focused Cleave" );
  talent.vengeance.soulmonger       = find_talent_spell( talent_tree::SPECIALIZATION, "Soulmonger" );
  talent.vengeance.stoke_the_flames = find_talent_spell( talent_tree::SPECIALIZATION, "Stoke the Flames" );
  talent.vengeance.burning_alive    = find_talent_spell( talent_tree::SPECIALIZATION, "Burning Alive" );
  talent.vengeance.cycle_of_binding = find_talent_spell( talent_tree::SPECIALIZATION, "Cycle of Binding" );

  talent.vengeance.vulnerability  = find_talent_spell( talent_tree::SPECIALIZATION, "Vulnerability" );
  talent.vengeance.feed_the_demon = find_talent_spell( talent_tree::SPECIALIZATION, "Feed the Demon" );
  talent.vengeance.charred_flesh  = find_talent_spell( talent_tree::SPECIALIZATION, "Charred Flesh" );

  talent.vengeance.soulcrush          = find_talent_spell( talent_tree::SPECIALIZATION, "Soulcrush" );
  talent.vengeance.soul_carver        = find_talent_spell( talent_tree::SPECIALIZATION, "Soul Carver" );
  talent.vengeance.last_resort        = find_talent_spell( talent_tree::SPECIALIZATION, "Last Resort" );
  talent.vengeance.darkglare_boon     = find_talent_spell( talent_tree::SPECIALIZATION, "Darkglare Boon" );
  talent.vengeance.down_in_flames     = find_talent_spell( talent_tree::SPECIALIZATION, "Down in Flames" );
  talent.vengeance.illuminated_sigils = find_talent_spell( talent_tree::SPECIALIZATION, "Illuminated Sigils" );

  // Hero Talents ===========================================================

  // Aldrachi Reaver talents
  talent.aldrachi_reaver.art_of_the_glaive = find_talent_spell( talent_tree::HERO, "Art of the Glaive" );

  talent.aldrachi_reaver.keen_engagement    = find_talent_spell( talent_tree::HERO, "Keen Engagement" );
  talent.aldrachi_reaver.preemptive_strike  = find_talent_spell( talent_tree::HERO, "Preemptive Strike" );
  talent.aldrachi_reaver.evasive_action     = find_talent_spell( talent_tree::HERO, "Evasive Action" );
  talent.aldrachi_reaver.unhindered_assault = find_talent_spell( talent_tree::HERO, "Unhindered Assault" );
  talent.aldrachi_reaver.incisive_blade     = find_talent_spell( talent_tree::HERO, "Incisive Blade" );

  talent.aldrachi_reaver.aldrachi_tactics     = find_talent_spell( talent_tree::HERO, "Aldrachi Tactics" );
  talent.aldrachi_reaver.army_unto_oneself    = find_talent_spell( talent_tree::HERO, "Army Unto Oneself" );
  talent.aldrachi_reaver.incorruptible_spirit = find_talent_spell( talent_tree::HERO, "Incorruptible Spirit" );
  talent.aldrachi_reaver.wounded_quarry       = find_talent_spell( talent_tree::HERO, "Wounded Quarry" );

  talent.aldrachi_reaver.intent_pursuit   = find_talent_spell( talent_tree::HERO, "Intent Pursuit" );
  talent.aldrachi_reaver.escalation       = find_talent_spell( talent_tree::HERO, "Escalation" );
  talent.aldrachi_reaver.warblades_hunger = find_talent_spell( talent_tree::HERO, "Warblade's Hunger" );

  talent.aldrachi_reaver.thrill_of_the_fight = find_talent_spell( talent_tree::HERO, "Thrill of the Fight" );

  // Fel-Scarred talents
  talent.felscarred.demonsurge = find_talent_spell( talent_tree::HERO, "Demonsurge" );

  talent.felscarred.wave_of_debilitation  = find_talent_spell( talent_tree::HERO, "Wave of Debilitation" );
  talent.felscarred.pursuit_of_angriness  = find_talent_spell( talent_tree::HERO, "Pursuit of Angriness" );
  talent.felscarred.focused_hatred        = find_talent_spell( talent_tree::HERO, "Focused Hatred" );
  talent.felscarred.set_fire_to_the_pain  = find_talent_spell( talent_tree::HERO, "Set Fire to the Pain" );
  talent.felscarred.improved_soul_rending = find_talent_spell( talent_tree::HERO, "Improved Soul Rending" );

  talent.felscarred.burning_blades         = find_talent_spell( talent_tree::HERO, "Burning Blades" );
  talent.felscarred.violent_transformation = find_talent_spell( talent_tree::HERO, "Violent Transformation" );
  talent.felscarred.enduring_torment       = find_talent_spell( talent_tree::HERO, "Enduring Torment" );

  talent.felscarred.untethered_fury      = find_talent_spell( talent_tree::HERO, "Untethered Fury" );
  talent.felscarred.student_of_suffering = find_talent_spell( talent_tree::HERO, "Student of Suffering" );
  talent.felscarred.flamebound           = find_talent_spell( talent_tree::HERO, "Flamebound" );
  talent.felscarred.monster_rising       = find_talent_spell( talent_tree::HERO, "Monster Rising" );

  talent.felscarred.demonic_intensity = find_talent_spell( talent_tree::HERO, "Demonic Intensity" );

  // Class Background Spells
  spell.felblade_damage      = talent.demon_hunter.felblade->ok() ? find_spell( 213243 ) : spell_data_t::not_found();
  spell.felblade_reset_havoc = talent.demon_hunter.felblade->ok() ? find_spell( 236167 ) : spell_data_t::not_found();
  spell.felblade_reset_vengeance =
      talent.demon_hunter.felblade->ok() ? find_spell( 203557 ) : spell_data_t::not_found();
  spell.infernal_armor_damage =
      talent.demon_hunter.infernal_armor->ok() ? find_spell( 320334 ) : spell_data_t::not_found();
  spell.immolation_aura_damage = spell.immolation_aura_2->ok() ? find_spell( 258921 ) : spell_data_t::not_found();
  spell.sigil_of_flame_damage  = find_spell( 204598 );
  spell.sigil_of_flame_fury    = find_spell( 389787 );
  spell.the_hunt               = talent.demon_hunter.the_hunt;
  spec.sigil_of_misery_debuff =
      talent.demon_hunter.sigil_of_misery->ok() ? find_spell( 207685 ) : spell_data_t::not_found();

  // Spec Background Spells
  mastery.any_means_necessary = talent.havoc.any_means_necessary;
  mastery.any_means_necessary_tuning =
      talent.havoc.any_means_necessary->ok() ? find_spell( 394486 ) : spell_data_t::not_found();

  spec.burning_wound_debuff = talent.havoc.burning_wound->effectN( 1 ).trigger();
  spec.chaos_theory_buff    = talent.havoc.chaos_theory->ok() ? find_spell( 390195 ) : spell_data_t::not_found();
  spec.demon_blades_damage  = talent.havoc.demon_blades->effectN( 1 ).trigger();
  spec.essence_break_debuff = talent.havoc.essence_break->ok() ? find_spell( 320338 ) : spell_data_t::not_found();
  spec.eye_beam_damage      = talent.havoc.eye_beam->ok() ? find_spell( 198030 ) : spell_data_t::not_found();
  spec.furious_gaze_buff    = talent.havoc.furious_gaze->ok() ? find_spell( 343312 ) : spell_data_t::not_found();
  spec.first_blood_blade_dance_damage =
      talent.havoc.first_blood->ok() ? find_spell( 391374 ) : spell_data_t::not_found();
  spec.first_blood_blade_dance_2_damage =
      talent.havoc.first_blood->ok() ? find_spell( 391378 ) : spell_data_t::not_found();
  spec.first_blood_death_sweep_damage =
      talent.havoc.first_blood->ok() ? find_spell( 393055 ) : spell_data_t::not_found();
  spec.first_blood_death_sweep_2_damage =
      talent.havoc.first_blood->ok() ? find_spell( 393054 ) : spell_data_t::not_found();
  spec.glaive_tempest_damage  = talent.havoc.glaive_tempest->ok() ? find_spell( 342857 ) : spell_data_t::not_found();
  spec.initiative_buff        = talent.havoc.initiative->ok() ? find_spell( 391215 ) : spell_data_t::not_found();
  spec.inner_demon_buff       = talent.havoc.inner_demon->ok() ? find_spell( 390145 ) : spell_data_t::not_found();
  spec.inner_demon_damage     = talent.havoc.inner_demon->ok() ? find_spell( 390137 ) : spell_data_t::not_found();
  spec.momentum_buff          = talent.havoc.momentum->ok() ? find_spell( 208628 ) : spell_data_t::not_found();
  spec.inertia_buff           = talent.havoc.inertia->ok() ? find_spell( 427641 ) : spell_data_t::not_found();
  spec.ragefire_damage        = talent.havoc.ragefire->ok() ? find_spell( 390197 ) : spell_data_t::not_found();
  spec.restless_hunter_buff   = talent.havoc.restless_hunter->ok() ? find_spell( 390212 ) : spell_data_t::not_found();
  spec.serrated_glaive_debuff = talent.havoc.serrated_glaive->effectN( 1 ).trigger();
  spec.soulscar_debuff        = talent.havoc.soulscar->ok() ? find_spell( 390181 ) : spell_data_t::not_found();
  spec.tactical_retreat_buff  = talent.havoc.tactical_retreat->ok() ? find_spell( 389890 ) : spell_data_t::not_found();
  spec.unbound_chaos_buff     = talent.havoc.unbound_chaos->ok() ? find_spell( 347462 ) : spell_data_t::not_found();
  spec.chaotic_disposition_damage =
      talent.havoc.chaotic_disposition->ok() ? find_spell( 428493 ) : spell_data_t::not_found();

  spec.demon_spikes_buff  = find_spell( 203819 );
  spec.fiery_brand_debuff = talent.vengeance.fiery_brand->ok() ? find_spell( 207771 ) : spell_data_t::not_found();
  spec.frailty_debuff     = talent.vengeance.frailty->ok() ? find_spell( 247456 ) : spell_data_t::not_found();
  spec.painbringer_buff   = talent.vengeance.painbringer->ok() ? find_spell( 212988 ) : spell_data_t::not_found();
  spec.calcified_spikes_buff =
      talent.vengeance.calcified_spikes->ok() ? find_spell( 391171 ) : spell_data_t::not_found();
  spec.soul_furnace_damage_amp = talent.vengeance.soul_furnace->ok() ? find_spell( 391172 ) : spell_data_t::not_found();
  spec.soul_furnace_stack      = talent.vengeance.soul_furnace->ok() ? find_spell( 391166 ) : spell_data_t::not_found();
  spec.retaliation_damage      = talent.vengeance.retaliation->ok() ? find_spell( 391159 ) : spell_data_t::not_found();
  spec.sigil_of_silence_debuff =
      talent.vengeance.sigil_of_silence->ok() ? find_spell( 204490 ) : spell_data_t::not_found();
  spec.sigil_of_chains_debuff =
      talent.vengeance.sigil_of_chains->ok() ? find_spell( 204843 ) : spell_data_t::not_found();
  spec.burning_alive_controller =
      talent.vengeance.burning_alive->ok() ? find_spell( 207760 ) : spell_data_t::not_found();
  spec.infernal_strike_impact = find_spell( 189112 );
  spec.spirit_bomb_damage     = talent.vengeance.spirit_bomb->ok() ? find_spell( 247455 ) : spell_data_t::not_found();
  spec.frailty_heal           = talent.vengeance.frailty->ok() ? find_spell( 227255 ) : spell_data_t::not_found();
  spec.feast_of_souls_heal  = talent.vengeance.feast_of_souls->ok() ? find_spell( 207693 ) : spell_data_t::not_found();
  spec.fel_devastation_2    = find_rank_spell( "Fel Devastation", "Rank 2" );
  spec.fel_devastation_heal = talent.vengeance.fel_devastation->ok() ? find_spell( 212106 ) : spell_data_t::not_found();

  // Hero spec background spells
  hero_spec.reavers_glaive =
      talent.aldrachi_reaver.art_of_the_glaive->ok() ? find_spell( 442294 ) : spell_data_t::not_found();
  hero_spec.reavers_mark =
      talent.aldrachi_reaver.art_of_the_glaive->ok() ? find_spell( 442624 ) : spell_data_t::not_found();
  hero_spec.glaive_flurry =
      talent.aldrachi_reaver.art_of_the_glaive->ok() ? find_spell( 442435 ) : spell_data_t::not_found();
  hero_spec.rending_strike =
      talent.aldrachi_reaver.art_of_the_glaive->ok() ? find_spell( 442442 ) : spell_data_t::not_found();
  hero_spec.art_of_the_glaive_buff =
      talent.aldrachi_reaver.art_of_the_glaive->ok() ? find_spell( 444661 ) : spell_data_t::not_found();
  hero_spec.art_of_the_glaive_damage =
      talent.aldrachi_reaver.art_of_the_glaive->ok() ? find_spell( 444810 ) : spell_data_t::not_found();
  hero_spec.warblades_hunger_buff =
      talent.aldrachi_reaver.warblades_hunger->ok() ? find_spell( 442503 ) : spell_data_t::not_found();
  hero_spec.warblades_hunger_damage =
      talent.aldrachi_reaver.warblades_hunger->ok() ? find_spell( 442507 ) : spell_data_t::not_found();
  hero_spec.burning_blades_debuff =
      talent.felscarred.burning_blades->ok() ? find_spell( 453177 ) : spell_data_t::not_found();
  hero_spec.student_of_suffering_buff =
      talent.felscarred.student_of_suffering->ok() ? find_spell( 453239 ) : spell_data_t::not_found();
  hero_spec.monster_rising_buff =
      talent.felscarred.monster_rising->ok() ? find_spell( 452550 ) : spell_data_t::not_found();
  hero_spec.enduring_torment_buff =
      talent.felscarred.enduring_torment->ok() ? find_spell( 453314 ) : spell_data_t::not_found();
  hero_spec.demonsurge_demonic_buff =
      talent.felscarred.demonsurge->ok() ? find_spell( 452435 ) : spell_data_t::not_found();
  hero_spec.demonsurge_hardcast_buff =
      talent.felscarred.demonic_intensity->ok() ? find_spell( 452489 ) : spell_data_t::not_found();
  hero_spec.demonsurge_damage = talent.felscarred.demonsurge->ok() ? find_spell( 452416 ) : spell_data_t::not_found();
  hero_spec.demonsurge_stacking_buff =
      talent.felscarred.demonic_intensity->ok() ? find_spell( 452416 ) : spell_data_t::not_found();
  hero_spec.demonsurge_trigger = talent.felscarred.demonsurge->ok() ? find_spell( 453323 ) : spell_data_t::not_found();
  hero_spec.soul_sunder        = talent.felscarred.demonsurge->ok() ? find_spell( 452436 ) : spell_data_t::not_found();
  hero_spec.spirit_burst       = talent.felscarred.demonsurge->ok() ? find_spell( 452437 ) : spell_data_t::not_found();

  // Sigil overrides for Precise/Concentrated Sigils
  std::vector<const spell_data_t*> sigil_overrides = { talent.demon_hunter.precise_sigils };
  spell.sigil_of_flame                             = find_spell_override( find_spell( 204596 ), sigil_overrides );
  spell.sigil_of_spite = find_spell_override( talent.demon_hunter.sigil_of_spite, sigil_overrides );
  spell.sigil_of_spite_damage =
      talent.demon_hunter.sigil_of_spite->ok() ? find_spell( 389860 ) : spell_data_t::not_found();
  spec.sigil_of_misery  = find_spell_override( talent.demon_hunter.sigil_of_misery, sigil_overrides );
  spec.sigil_of_silence = find_spell_override( talent.vengeance.sigil_of_silence, sigil_overrides );
  spec.sigil_of_chains  = find_spell_override( talent.vengeance.sigil_of_chains, sigil_overrides );

  spell.fodder_to_the_flame = talent.demon_hunter.fodder_to_the_flame;
  spell.fodder_to_the_flame_damage =
      talent.demon_hunter.fodder_to_the_flame->ok() ? find_spell( 350631 ) : spell_data_t::not_found();

  if ( talent.demon_hunter.collective_anguish->ok() )
  {
    spell.collective_anguish = specialization() == DEMON_HUNTER_HAVOC ? find_spell( 393831 ) : find_spell( 391057 );
    spell.collective_anguish_damage =
        ( specialization() == DEMON_HUNTER_HAVOC ? spell.collective_anguish->effectN( 1 ).trigger()
                                                 : find_spell( 391058 ) );
  }
  else
  {
    spell.collective_anguish        = spell_data_t::not_found();
    spell.collective_anguish_damage = spell_data_t::not_found();
  }

  // Set Bonus Items ========================================================

  set_bonuses.t29_havoc_2pc      = sets->set( DEMON_HUNTER_HAVOC, T29, B2 );
  set_bonuses.t29_havoc_4pc      = sets->set( DEMON_HUNTER_HAVOC, T29, B4 );
  set_bonuses.t29_vengeance_2pc  = sets->set( DEMON_HUNTER_VENGEANCE, T29, B2 );
  set_bonuses.t29_vengeance_4pc  = sets->set( DEMON_HUNTER_VENGEANCE, T29, B4 );
  set_bonuses.t30_havoc_2pc      = sets->set( DEMON_HUNTER_HAVOC, T30, B2 );
  set_bonuses.t30_havoc_4pc      = sets->set( DEMON_HUNTER_HAVOC, T30, B4 );
  set_bonuses.t30_vengeance_2pc  = sets->set( DEMON_HUNTER_VENGEANCE, T30, B2 );
  set_bonuses.t30_vengeance_4pc  = sets->set( DEMON_HUNTER_VENGEANCE, T30, B4 );
  set_bonuses.t31_havoc_2pc      = sets->set( DEMON_HUNTER_HAVOC, T31, B2 );
  set_bonuses.t31_havoc_4pc      = sets->set( DEMON_HUNTER_HAVOC, T31, B4 );
  set_bonuses.t31_vengeance_2pc  = sets->set( DEMON_HUNTER_VENGEANCE, T31, B2 );
  set_bonuses.t31_vengeance_4pc  = sets->set( DEMON_HUNTER_VENGEANCE, T31, B4 );
  set_bonuses.tww1_havoc_2pc     = sets->set( DEMON_HUNTER_HAVOC, TWW1, B2 );
  set_bonuses.tww1_havoc_4pc     = sets->set( DEMON_HUNTER_HAVOC, TWW1, B4 );
  set_bonuses.tww1_vengeance_2pc = sets->set( DEMON_HUNTER_VENGEANCE, TWW1, B2 );
  set_bonuses.tww1_vengeance_4pc = sets->set( DEMON_HUNTER_VENGEANCE, TWW1, B4 );

  // Set Bonus Auxilliary
  set_bonuses.t29_vengeance_4pc_debuff =
      set_bonuses.t29_vengeance_4pc->ok() ? find_spell( 394958 ) : spell_data_t::not_found();
  set_bonuses.t30_havoc_2pc_buff   = set_bonuses.t30_havoc_2pc->ok() ? find_spell( 408737 ) : spell_data_t::not_found();
  set_bonuses.t30_havoc_4pc_buff   = set_bonuses.t30_havoc_4pc->ok() ? find_spell( 408754 ) : spell_data_t::not_found();
  set_bonuses.t30_havoc_4pc_refund = set_bonuses.t30_havoc_4pc->ok() ? find_spell( 408757 ) : spell_data_t::not_found();
  set_bonuses.t30_vengeance_2pc_buff =
      set_bonuses.t30_vengeance_2pc->ok() ? find_spell( 409645 ) : spell_data_t::not_found();
  set_bonuses.t30_vengeance_4pc_buff =
      set_bonuses.t30_vengeance_4pc->ok() ? find_spell( 409877 ) : spell_data_t::not_found();
  set_bonuses.t31_vengeance_2pc_buff =
      set_bonuses.t31_vengeance_2pc->ok() ? find_spell( 425653 ) : spell_data_t::not_found();
  set_bonuses.t31_vengeance_4pc_proc =
      set_bonuses.t31_vengeance_4pc->ok() ? find_spell( 425672 ) : spell_data_t::not_found();
  set_bonuses.tww1_havoc_4pc_buff = set_bonuses.tww1_havoc_4pc->ok() ? find_spell( 454628 ) : spell_data_t::not_found();
  set_bonuses.tww1_vengeance_4pc_buff =
      set_bonuses.tww1_vengeance_4pc->ok() ? find_spell( 454774 ) : spell_data_t::not_found();

  // Spell Initialization ===================================================

  using namespace actions::attacks;
  using namespace actions::spells;
  using namespace actions::heals;

  active.consume_soul_greater =
      new consume_soul_t( this, "consume_soul_greater", spec.consume_soul_greater, soul_fragment::GREATER );
  active.consume_soul_lesser =
      new consume_soul_t( this, "consume_soul_lesser", spec.consume_soul_lesser, soul_fragment::LESSER );

  active.burning_wound = get_background_action<burning_wound_t>( "burning_wound" );

  if ( talent.demon_hunter.fodder_to_the_flame->ok() )
  {
    active.fodder_to_the_flame_damage =
        new spectral_sight_t::fodder_to_the_flame_damage_t( "fodder_to_the_flame", this );
  }

  if ( talent.havoc.chaotic_disposition->ok() )
  {
    auto chaotic_disposition_effect          = new special_effect_t( this );
    chaotic_disposition_effect->name_str     = "chaotic_disposition";
    chaotic_disposition_effect->type         = SPECIAL_EFFECT_EQUIP;
    chaotic_disposition_effect->spell_id     = talent.havoc.chaotic_disposition->id();
    chaotic_disposition_effect->proc_flags2_ = PF2_ALL_HIT | PF2_PERIODIC_DAMAGE;
    chaotic_disposition_effect->proc_chance_ = 1.0;  // 2023-11-14 -- Proc chance removed from talent spell
    special_effects.push_back( chaotic_disposition_effect );

    auto chaotic_disposition_cb = new chaotic_disposition_cb_t( this, *chaotic_disposition_effect );

    chaotic_disposition_cb->activate();
  }

  if ( talent.havoc.demon_blades->ok() )
  {
    active.demon_blades = new demon_blades_t( this );
  }

  if ( talent.demon_hunter.collective_anguish->ok() )
  {
    active.collective_anguish = get_background_action<collective_anguish_t>( "collective_anguish" );
  }

  if ( talent.havoc.relentless_onslaught->ok() )
  {
    active.relentless_onslaught = get_background_action<chaos_strike_t>( "chaos_strike_onslaught", "", true );
    active.relentless_onslaught_annihilation =
        get_background_action<annihilation_t>( "annihilation_onslaught", "", true );
  }

  if ( talent.havoc.inner_demon->ok() )
  {
    active.inner_demon = get_background_action<inner_demon_t>( "inner_demon" );
  }

  if ( talent.havoc.ragefire->ok() )
  {
    active.ragefire = get_background_action<ragefire_t>( "ragefire" );
  }

  if ( talent.vengeance.retaliation->ok() )
  {
    active.retaliation = get_background_action<retaliation_t>( "retaliation" );
  }

  if ( set_bonuses.t30_vengeance_4pc->ok() && talent.vengeance.fiery_brand->ok() )
  {
    fiery_brand_t* fiery_brand_t30 = get_background_action<fiery_brand_t>( "fiery_brand_t30", "", true );
    fiery_brand_t30->internal_cooldown->base_duration = 0_s;
    fiery_brand_t30->cooldown->base_duration          = 0_s;
    fiery_brand_t30->cooldown->charges                = 0;
    active.fiery_brand_t30                            = fiery_brand_t30;
  }

  if ( set_bonuses.t31_vengeance_4pc_proc->ok() )
  {
    active.sigil_of_flame_t31 = get_background_action<sigil_of_flame_t31_t>( "sigil_of_flame_t31" );
  }

  if ( set_bonuses.t31_havoc_2pc->ok() )
  {
    throw_glaive_t* throw_glaive_t31_proc =
        get_background_action<throw_glaive_t>( "throw_glaive_t31_proc", "", throw_glaive_t::glaive_source::T31_PROC );
    throw_glaive_t31_proc->cooldown->charges  = 0;
    throw_glaive_t31_proc->cooldown->duration = 0_ms;
    active.throw_glaive_t31_proc              = throw_glaive_t31_proc;

    throw_glaive_t* throw_glaive_bd_throw = get_background_action<throw_glaive_t>(
        "throw_glaive_bd_throw", "", throw_glaive_t::glaive_source::BLADE_DANCE_THROW );
    active.throw_glaive_bd_throw = throw_glaive_bd_throw;

    throw_glaive_t* throw_glaive_ds_throw = get_background_action<throw_glaive_t>(
        "throw_glaive_ds_throw", "", throw_glaive_t::glaive_source::DEATH_SWEEP_THROW );
    active.throw_glaive_ds_throw = throw_glaive_ds_throw;
  }

  if ( talent.aldrachi_reaver.art_of_the_glaive->ok() )
  {
    active.art_of_the_glaive = get_background_action<art_of_the_glaive_t>( "art_of_the_glaive" );
  }
  if ( talent.aldrachi_reaver.preemptive_strike->ok() )
  {
    active.preemptive_strike = get_background_action<preemptive_strike_t>( "preemptive_strike" );
  }
  if ( talent.aldrachi_reaver.warblades_hunger->ok() )
  {
    active.warblades_hunger = get_background_action<warblades_hunger_t>( "warblades_hunger" );
  }

  if ( talent.felscarred.burning_blades->ok() )
  {
    active.burning_blades = get_background_action<burning_blades_t>( "burning_blades" );
  }
  if ( talent.felscarred.demonsurge->ok() )
  {
    active.demonsurge = get_background_action<demonsurge_t>( "demonsurge" );
  }
}



}  // namespace demon_hunter