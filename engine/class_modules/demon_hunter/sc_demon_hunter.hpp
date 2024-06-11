// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#pragma once

#include "action/parse_effects.hpp"

#include "simulationcraft.hpp"

namespace demon_hunter
{
using data_t        = std::pair<std::string, simple_sample_data_with_min_max_t>;
using simple_data_t = std::pair<std::string, simple_sample_data_t>;

struct demon_hunter_t;
struct demon_hunter_td_t;

constexpr unsigned MAX_SOUL_FRAGMENTS = 5;
enum class soul_fragment : unsigned
{
  GREATER         = 0x01,
  LESSER          = 0x02,
  GREATER_DEMON   = 0x04,
  EMPOWERED_DEMON = 0x08,

  ANY_GREATER = ( GREATER | GREATER_DEMON | EMPOWERED_DEMON ),
  ANY_DEMON   = ( GREATER_DEMON | EMPOWERED_DEMON ),
  ANY         = 0xFF
};

namespace actions
{
enum demonsurge_ability
{
  SOUL_SUNDER,
  SPIRIT_BURST,
  SIGIL_OF_DOOM,
  CONSUMING_FIRE,
  FEL_DESOLATION,
  ABYSSAL_GAZE,
  ANNIHILATION,
  DEATH_SWEEP
};

}  // namespace actions

namespace buffs
{
struct movement_buff_t : public buff_t
{
  double yards_from_melee;
  double distance_moved;
  demon_hunter_t* dh;

  movement_buff_t( demon_hunter_t* p, util::string_view name, const spell_data_t* spell_data = spell_data_t::nil(),
                   const item_t* item = nullptr );

  bool trigger( int s = 1, double v = DEFAULT_VALUE(), double c = -1.0, timespan_t d = timespan_t::min() ) override;
};
}  // namespace buffs

namespace events
{
}  // namespace events

namespace items
{
}  // namespace items

namespace pets
{
}  // namespace pets

struct demon_hunter_td_t : public actor_target_data_t
{
public:
  struct dots_t
  {
    // Shared
    dot_t* the_hunt;

    // Havoc
    dot_t* burning_wound;
    dot_t* trail_of_ruin;

    // Vengeance
    dot_t* fiery_brand;
    dot_t* sigil_of_flame;

  } dots;

  struct debuffs_t
  {
    // Shared
    buff_t* sigil_of_flame;

    // Havoc
    buff_t* burning_wound;
    buff_t* essence_break;
    buff_t* initiative_tracker;
    buff_t* serrated_glaive;

    // Vengeance
    buff_t* frailty;

    // Aldrachi Reaver
    buff_t* reavers_mark;

    // Set Bonuses
    buff_t* t29_vengeance_4pc;
  } debuffs;

  demon_hunter_td_t( player_t* target, demon_hunter_t& p );
  void trigger_burning_blades( action_state_t* state );
};

struct soul_fragment_t
{
public:
  struct fragment_expiration_t : public event_t
  {
    soul_fragment_t* frag;

    fragment_expiration_t( soul_fragment_t* s );
  };

  struct fragment_activate_t : public event_t
  {
    soul_fragment_t* frag;

    fragment_activate_t( soul_fragment_t* s );
  };

  demon_hunter_t* dh;
  double x, y;
  event_t* activate;
  event_t* expiration;
  const soul_fragment type;
  bool consume_on_activation;

  soul_fragment_t( demon_hunter_t* p, soul_fragment t, bool consume_on_activation );

  double get_distance( demon_hunter_t* p ) const;
  timespan_t get_travel_time( bool activation = false ) const;
  bool active() const;
  void remove() const;
  timespan_t remains() const;
  bool is_type( soul_fragment type_mask ) const;
  void set_position();
  void schedule_activate();
  void consume( bool heal = true, bool instant = false );
};

struct demon_hunter_t : public parse_player_effects_t
{
public:
  using base_t = parse_player_effects_t;

  // Data collection for cooldown waste
  auto_dispose<std::vector<data_t*>> cd_waste_exec, cd_waste_cumulative;
  auto_dispose<std::vector<simple_data_t*>> cd_waste_iter;

  double metamorphosis_health;  // Vengeance temp health from meta;
  double expected_max_health;

  // Soul Fragments
  unsigned next_fragment_spawn;  // determines whether the next fragment spawn
                                 // on the left or right
  auto_dispose<std::vector<soul_fragment_t*>> soul_fragments;
  event_t* soul_fragment_pick_up;

  double frailty_accumulator;  // Frailty healing accumulator
  event_t* frailty_driver;

  bool fodder_initiative;
  double shattered_destiny_accumulator;

  event_t* exit_melee_event;  // Event to disable melee abilities mid-VR.

  // Buffs
  struct buffs_t
  {
    // General
    buff_t* demon_soul;
    buff_t* empowered_demon_soul;
    buff_t* immolation_aura;
    buff_t* metamorphosis;
    buff_t* fodder_to_the_flame;

    // Havoc
    buff_t* blind_fury;
    buff_t* blur;
    buff_t* chaos_theory;
    buff_t* death_sweep;
    buff_t* fel_barrage;
    buff_t* furious_gaze;
    buff_t* inertia;
    buff_t* initiative;
    buff_t* inner_demon;
    buff_t* momentum;
    buff_t* out_of_range;
    buff_t* restless_hunter;
    buff_t* tactical_retreat;
    buff_t* unbound_chaos;

    buffs::movement_buff_t* fel_rush_move;
    buffs::movement_buff_t* vengeful_retreat_move;
    buffs::movement_buff_t* metamorphosis_move;

    // Vengeance
    buff_t* demon_spikes;
    buff_t* calcified_spikes;
    buff_t* painbringer;
    absorb_buff_t* soul_barrier;
    buff_t* soul_furnace_damage_amp;
    buff_t* soul_furnace_stack;
    buff_t* soul_fragments;

    // Aldrachi Reaver
    buff_t* art_of_the_glaive;
    buff_t* glaive_flurry;
    buff_t* rending_strike;
    buff_t* warblades_hunger;

    // Fel-scarred
    buff_t* monster_rising;
    buff_t* student_of_suffering;
    buff_t* enduring_torment;
    buff_t* pursuit_of_angryness;  // passive periodic updater buff
    std::unordered_map<actions::demonsurge_ability, buff_t*> demonsurge_abilities;
    buff_t* demonsurge_demonic;
    buff_t* demonsurge_hardcast;
    buff_t* demonsurge;

    // Set Bonuses
    buff_t* t29_havoc_4pc;
    buff_t* t30_havoc_2pc;
    buff_t* t30_havoc_4pc;
    buff_t* t30_vengeance_2pc;
    buff_t* t30_vengeance_4pc;
    buff_t* t31_vengeance_2pc;
    buff_t* tww1_havoc_4pc;
    buff_t* tww1_vengeance_4pc;
  } buff;

  // Talents
  struct talents_t
  {
    struct class_talents_t
    {
      player_talent_t vengeful_retreat;
      player_talent_t blazing_path;
      player_talent_t sigil_of_misery;  // NYI

      player_talent_t unrestrained_fury;
      player_talent_t imprison;               // No Implementation
      player_talent_t shattered_restoration;  // NYI Vengeance

      player_talent_t vengeful_bonds;  // No Implementation
      player_talent_t improved_disrupt;
      player_talent_t bouncing_glaives;
      player_talent_t consume_magic;
      player_talent_t improved_sigil_of_misery;

      player_talent_t pursuit;
      player_talent_t disrupting_fury;
      player_talent_t felblade;
      player_talent_t swallowed_anger;
      player_talent_t charred_warblades;  // NYI Vengeance

      player_talent_t felfire_haste;  // NYI
      player_talent_t master_of_the_glaive;
      player_talent_t champion_of_the_glaive;
      player_talent_t aura_of_pain;
      player_talent_t precise_sigils;    // Partial NYI (debuff Sigils)
      player_talent_t lost_in_darkness;  // No Implementation

      player_talent_t chaos_nova;
      player_talent_t soul_rending;
      player_talent_t infernal_armor;
      player_talent_t aldrachi_design;

      player_talent_t chaos_fragments;
      player_talent_t illidari_knowledge;
      player_talent_t demonic;
      player_talent_t will_of_the_illidari;  // NYI Vengeance
      player_talent_t live_by_the_glaive;    // NYI

      player_talent_t internal_struggle;
      player_talent_t darkness;  // No Implementation
      player_talent_t soul_sigils;
      player_talent_t quickened_sigils;

      player_talent_t erratic_felheart;
      player_talent_t long_night;   // No Implementation
      player_talent_t pitch_black;  // No Implementation
      player_talent_t rush_of_chaos;
      player_talent_t demon_muzzle;  // NYI Vengeance
      player_talent_t flames_of_fury;

      player_talent_t collective_anguish;
      player_talent_t fodder_to_the_flame;
      player_talent_t the_hunt;
      player_talent_t sigil_of_spite;

    } demon_hunter;

    struct havoc_talents_t
    {
      player_talent_t eye_beam;

      player_talent_t critical_chaos;
      player_talent_t insatiable_hunger;
      player_talent_t demon_blades;
      player_talent_t burning_hatred;

      player_talent_t improved_fel_rush;
      player_talent_t dash_of_chaos;  // NYI
      player_talent_t improved_chaos_strike;
      player_talent_t first_blood;
      player_talent_t accelerated_blade;
      player_talent_t demon_hide;

      player_talent_t desperate_instincts;  // No Implementation
      player_talent_t netherwalk;           // No Implementation
      player_talent_t deflecting_dance;     // NYI
      player_talent_t mortal_dance;         // No Implementation

      player_talent_t initiative;
      player_talent_t scars_of_suffering;
      player_talent_t chaotic_transformation;
      player_talent_t furious_throws;
      player_talent_t trail_of_ruin;

      player_talent_t unbound_chaos;
      player_talent_t blind_fury;
      player_talent_t looks_can_kill;
      player_talent_t dancing_with_fate;
      player_talent_t growing_inferno;

      player_talent_t tactical_retreat;
      player_talent_t isolated_prey;
      player_talent_t furious_gaze;
      player_talent_t relentless_onslaught;
      player_talent_t burning_wound;

      player_talent_t momentum;
      player_talent_t inertia;  // NYI
      player_talent_t chaos_theory;
      player_talent_t restless_hunter;
      player_talent_t inner_demon;
      player_talent_t serrated_glaive;  // Partially implemented
      player_talent_t ragefire;

      player_talent_t know_your_enemy;
      player_talent_t glaive_tempest;
      player_talent_t cycle_of_hatred;
      player_talent_t soulscar;
      player_talent_t chaotic_disposition;

      player_talent_t essence_break;
      player_talent_t fel_barrage;  // Old implementation
      player_talent_t shattered_destiny;
      player_talent_t any_means_necessary;
      player_talent_t a_fire_inside;

    } havoc;

    struct vengeance_talents_t
    {
      player_talent_t fel_devastation;

      player_talent_t frailty;
      player_talent_t fiery_brand;

      player_talent_t perfectly_balanced_glaive;
      player_talent_t deflecting_spikes;
      player_talent_t ascending_flame;

      player_talent_t shear_fury;
      player_talent_t fracture;
      player_talent_t calcified_spikes;
      player_talent_t roaring_fire;      // No Implementation
      player_talent_t sigil_of_silence;  // Partial Implementation
      player_talent_t retaliation;
      player_talent_t meteoric_strikes;

      player_talent_t spirit_bomb;
      player_talent_t feast_of_souls;
      player_talent_t agonizing_flames;
      player_talent_t extended_spikes;
      player_talent_t burning_blood;
      player_talent_t soul_barrier;  // NYI
      player_talent_t bulk_extraction;
      player_talent_t revel_in_pain;  // No Implementation

      player_talent_t void_reaver;
      player_talent_t fallout;
      player_talent_t ruinous_bulwark;  // No Implementation
      player_talent_t volatile_flameblood;
      player_talent_t fel_flame_fortification;  // No Implementation

      player_talent_t soul_furnace;
      player_talent_t painbringer;
      player_talent_t sigil_of_chains;  // Partial Implementation
      player_talent_t fiery_demise;
      player_talent_t chains_of_anger;

      player_talent_t focused_cleave;
      player_talent_t soulmonger;  // No Implementation
      player_talent_t stoke_the_flames;
      player_talent_t burning_alive;
      player_talent_t cycle_of_binding;

      player_talent_t vulnerability;
      player_talent_t feed_the_demon;
      player_talent_t charred_flesh;

      player_talent_t soulcrush;
      player_talent_t soul_carver;
      player_talent_t last_resort;  // NYI
      player_talent_t darkglare_boon;
      player_talent_t down_in_flames;
      player_talent_t illuminated_sigils;  // Partial Implementation

    } vengeance;

    struct aldrachi_reaver_talents_t
    {
      player_talent_t art_of_the_glaive;

      player_talent_t keen_engagement;
      player_talent_t preemptive_strike;
      player_talent_t evasive_action;  // No Implementation
      player_talent_t unhindered_assault;
      player_talent_t incisive_blade;  // NYI - bugged in-game

      player_talent_t aldrachi_tactics;      // NYI
      player_talent_t army_unto_oneself;     // NYI
      player_talent_t incorruptible_spirit;  // NYI
      player_talent_t wounded_quarry;        // NYI

      player_talent_t intent_pursuit;    // partially implemented (Blade Dance / Soul Cleave)
      player_talent_t escalation;        // NYI
      player_talent_t warblades_hunger;  // NYI

      player_talent_t thrill_of_the_fight;  // NYI
    } aldrachi_reaver;

    struct felscarred_talents_t
    {
      player_talent_t demonsurge;  // partially implemented

      player_talent_t wave_of_debilitation;  // No Implementation
      player_talent_t pursuit_of_angriness;
      player_talent_t focused_hatred;
      player_talent_t set_fire_to_the_pain;  // NYI
      player_talent_t improved_soul_rending;

      player_talent_t burning_blades;
      player_talent_t violent_transformation;
      player_talent_t enduring_torment;  // NYI for Vengeance

      player_talent_t untethered_fury;
      player_talent_t student_of_suffering;
      player_talent_t flamebound;
      player_talent_t monster_rising;

      player_talent_t demonic_intensity;
    } felscarred;
  } talent;

  // Spell Data
  struct spells_t
  {
    // Core Class Spells
    const spell_data_t* chaos_brand;
    const spell_data_t* disrupt;
    const spell_data_t* immolation_aura;
    const spell_data_t* throw_glaive;
    const spell_data_t* sigil_of_flame;
    const spell_data_t* spectral_sight;

    // Class Passives
    const spell_data_t* all_demon_hunter;
    const spell_data_t* critical_strikes;
    const spell_data_t* immolation_aura_2;
    const spell_data_t* leather_specialization;

    // Background Spells
    const spell_data_t* collective_anguish;
    const spell_data_t* collective_anguish_damage;
    const spell_data_t* demon_soul;
    const spell_data_t* demon_soul_empowered;
    const spell_data_t* felblade_damage;
    const spell_data_t* felblade_reset_havoc;
    const spell_data_t* felblade_reset_vengeance;
    const spell_data_t* immolation_aura_damage;
    const spell_data_t* infernal_armor_damage;
    const spell_data_t* sigil_of_flame_damage;
    const spell_data_t* sigil_of_flame_fury;
    const spell_data_t* soul_fragment;

    // Cross-Expansion Override Spells
    const spell_data_t* sigil_of_spite;
    const spell_data_t* sigil_of_spite_damage;
    const spell_data_t* fodder_to_the_flame;
    const spell_data_t* fodder_to_the_flame_damage;
    const spell_data_t* the_hunt;
  } spell;

  // Specialization Spells
  struct spec_t
  {
    // General
    const spell_data_t* consume_soul_greater;
    const spell_data_t* consume_soul_lesser;
    const spell_data_t* demonic_wards;
    const spell_data_t* metamorphosis;
    const spell_data_t* metamorphosis_buff;
    const spell_data_t* sigil_of_misery;
    const spell_data_t* sigil_of_misery_debuff;

    // Havoc
    const spell_data_t* havoc_demon_hunter;
    const spell_data_t* annihilation;
    const spell_data_t* blade_dance;
    const spell_data_t* blur;
    const spell_data_t* chaos_strike;
    const spell_data_t* death_sweep;
    const spell_data_t* demons_bite;
    const spell_data_t* fel_rush;
    const spell_data_t* fel_eruption;

    const spell_data_t* blade_dance_2;
    const spell_data_t* burning_wound_debuff;
    const spell_data_t* chaos_strike_fury;
    const spell_data_t* chaos_strike_refund;
    const spell_data_t* chaos_theory_buff;
    const spell_data_t* demon_blades_damage;
    const spell_data_t* demonic_appetite;
    const spell_data_t* demonic_appetite_fury;
    const spell_data_t* essence_break_debuff;
    const spell_data_t* eye_beam_damage;
    const spell_data_t* fel_rush_damage;
    const spell_data_t* first_blood_blade_dance_damage;
    const spell_data_t* first_blood_blade_dance_2_damage;
    const spell_data_t* first_blood_death_sweep_damage;
    const spell_data_t* first_blood_death_sweep_2_damage;
    const spell_data_t* furious_gaze_buff;
    const spell_data_t* glaive_tempest_damage;
    const spell_data_t* immolation_aura_3;
    const spell_data_t* initiative_buff;
    const spell_data_t* inner_demon_buff;
    const spell_data_t* inner_demon_damage;
    const spell_data_t* momentum_buff;
    const spell_data_t* inertia_buff;
    const spell_data_t* ragefire_damage;
    const spell_data_t* serrated_glaive_debuff;
    const spell_data_t* soulscar_debuff;
    const spell_data_t* restless_hunter_buff;
    const spell_data_t* tactical_retreat_buff;
    const spell_data_t* unbound_chaos_buff;
    const spell_data_t* chaotic_disposition_damage;

    // Vengeance
    const spell_data_t* vengeance_demon_hunter;
    const spell_data_t* demon_spikes;
    const spell_data_t* infernal_strike;
    const spell_data_t* soul_cleave;
    const spell_data_t* shear;

    const spell_data_t* demonic_wards_2;
    const spell_data_t* demonic_wards_3;
    const spell_data_t* fiery_brand_debuff;
    const spell_data_t* frailty_debuff;
    const spell_data_t* riposte;
    const spell_data_t* soul_cleave_2;
    const spell_data_t* thick_skin;
    const spell_data_t* demon_spikes_buff;
    const spell_data_t* painbringer_buff;
    const spell_data_t* calcified_spikes_buff;
    const spell_data_t* soul_furnace_damage_amp;
    const spell_data_t* soul_furnace_stack;
    const spell_data_t* immolation_aura_cdr;
    const spell_data_t* soul_fragments_buff;
    const spell_data_t* retaliation_damage;
    const spell_data_t* sigil_of_silence;
    const spell_data_t* sigil_of_silence_debuff;
    const spell_data_t* sigil_of_chains;
    const spell_data_t* sigil_of_chains_debuff;
    const spell_data_t* burning_alive_controller;
    const spell_data_t* infernal_strike_impact;
    const spell_data_t* spirit_bomb_damage;
    const spell_data_t* frailty_heal;
    const spell_data_t* feast_of_souls_heal;
    const spell_data_t* fel_devastation_2;
    const spell_data_t* fel_devastation_heal;
  } spec;

  struct hero_spec_t
  {
    // Aldrachi Reaver
    const spell_data_t* reavers_glaive;
    const spell_data_t* reavers_mark;
    const spell_data_t* glaive_flurry;
    const spell_data_t* rending_strike;
    const spell_data_t* art_of_the_glaive_buff;
    const spell_data_t* art_of_the_glaive_damage;
    const spell_data_t* warblades_hunger_buff;
    const spell_data_t* warblades_hunger_damage;

    // Fel-scarred
    const spell_data_t* burning_blades_debuff;
    const spell_data_t* enduring_torment_buff;
    const spell_data_t* monster_rising_buff;
    const spell_data_t* student_of_suffering_buff;
    const spell_data_t* demonsurge_demonic_buff;
    const spell_data_t* demonsurge_hardcast_buff;
    const spell_data_t* demonsurge_damage;
    const spell_data_t* demonsurge_stacking_buff;
    const spell_data_t* demonsurge_trigger;
    const spell_data_t* soul_sunder;
    const spell_data_t* spirit_burst;
  } hero_spec;

  // Set Bonus effects
  struct set_bonuses_t
  {
    const spell_data_t* t29_havoc_2pc;
    const spell_data_t* t29_havoc_4pc;
    const spell_data_t* t29_vengeance_2pc;
    const spell_data_t* t29_vengeance_4pc;
    const spell_data_t* t30_havoc_2pc;
    const spell_data_t* t30_havoc_4pc;
    const spell_data_t* t30_vengeance_2pc;
    const spell_data_t* t30_vengeance_4pc;
    const spell_data_t* t31_havoc_2pc;
    const spell_data_t* t31_havoc_4pc;
    const spell_data_t* t31_vengeance_2pc;
    const spell_data_t* t31_vengeance_4pc;
    const spell_data_t* tww1_havoc_2pc;
    const spell_data_t* tww1_havoc_4pc;
    const spell_data_t* tww1_vengeance_2pc;
    const spell_data_t* tww1_vengeance_4pc;

    // Auxilliary
    const spell_data_t* t29_vengeance_4pc_debuff;
    const spell_data_t* t30_havoc_2pc_buff;
    const spell_data_t* t30_havoc_4pc_refund;
    const spell_data_t* t30_havoc_4pc_buff;
    double t30_havoc_2pc_fury_tracker = 0.0;
    const spell_data_t* t30_vengeance_2pc_buff;
    const spell_data_t* t30_vengeance_4pc_buff;
    double t30_vengeance_4pc_soul_fragments_tracker = 0;
    double t31_vengeance_4pc_fury_tracker           = 0;
    const spell_data_t* t31_vengeance_2pc_buff;
    const spell_data_t* t31_vengeance_4pc_proc;
    const spell_data_t* tww1_havoc_4pc_buff;
    const spell_data_t* tww1_vengeance_4pc_buff;
  } set_bonuses;

  // Mastery Spells
  struct mastery_t
  {
    // Havoc
    const spell_data_t* demonic_presence;
    const spell_data_t* any_means_necessary;
    const spell_data_t* any_means_necessary_tuning;
    // Vengeance
    const spell_data_t* fel_blood;
    const spell_data_t* fel_blood_rank_2;
  } mastery;

  // Cooldowns
  struct cooldowns_t
  {
    // General
    cooldown_t* consume_magic;
    cooldown_t* disrupt;
    cooldown_t* sigil_of_spite;
    cooldown_t* felblade;
    cooldown_t* fel_eruption;
    cooldown_t* immolation_aura;
    cooldown_t* the_hunt;
    cooldown_t* spectral_sight;
    cooldown_t* sigil_of_flame;
    cooldown_t* sigil_of_misery;
    cooldown_t* metamorphosis;
    cooldown_t* throw_glaive;
    cooldown_t* vengeful_retreat;
    cooldown_t* chaos_nova;

    // Havoc
    cooldown_t* blade_dance;
    cooldown_t* blur;
    cooldown_t* chaos_strike_refund_icd;
    cooldown_t* essence_break;
    cooldown_t* demonic_appetite;
    cooldown_t* eye_beam;
    cooldown_t* fel_barrage;
    cooldown_t* fel_rush;
    cooldown_t* netherwalk;
    cooldown_t* relentless_onslaught_icd;
    cooldown_t* movement_shared;

    // Vengeance
    cooldown_t* demon_spikes;
    cooldown_t* fiery_brand;
    cooldown_t* fel_devastation;
    cooldown_t* sigil_of_chains;
    cooldown_t* sigil_of_silence;
    cooldown_t* volatile_flameblood_icd;

    // Fel-scarred
    cooldown_t* soul_cleave;
  } cooldown;

  // Gains
  struct gains_t
  {
    // General
    gain_t* miss_refund;
    gain_t* immolation_aura;

    // Havoc
    gain_t* blind_fury;
    gain_t* demonic_appetite;
    gain_t* tactical_retreat;

    // Vengeance
    gain_t* metamorphosis;
    gain_t* volatile_flameblood;
    gain_t* darkglare_boon;

    // Set Bonuses
    gain_t* seething_fury;

    // Fel-scarred
    gain_t* student_of_suffering;
  } gain;

  // Benefits
  struct benefits_t
  {
  } benefits;

  // Procs
  struct procs_t
  {
    // General
    proc_t* delayed_aa_range;
    proc_t* delayed_aa_channel;
    proc_t* soul_fragment_greater;
    proc_t* soul_fragment_greater_demon;
    proc_t* soul_fragment_empowered_demon;
    proc_t* soul_fragment_lesser;
    proc_t* felblade_reset;

    // Havoc
    proc_t* demonic_appetite;
    proc_t* demons_bite_in_meta;
    proc_t* chaos_strike_in_essence_break;
    proc_t* annihilation_in_essence_break;
    proc_t* blade_dance_in_essence_break;
    proc_t* death_sweep_in_essence_break;
    proc_t* chaos_strike_in_serrated_glaive;
    proc_t* annihilation_in_serrated_glaive;
    proc_t* throw_glaive_in_serrated_glaive;
    proc_t* shattered_destiny;
    proc_t* eye_beam_canceled;

    // Vengeance
    proc_t* soul_fragment_expire;
    proc_t* soul_fragment_overflow;
    proc_t* soul_fragment_from_shear;
    proc_t* soul_fragment_from_fracture;
    proc_t* soul_fragment_from_sigil_of_spite;
    proc_t* soul_fragment_from_fallout;
    proc_t* soul_fragment_from_meta;

    // Set Bonuses
    proc_t* soul_fragment_from_t29_2pc;
    proc_t* soul_fragment_from_t31_4pc;
    proc_t* soul_fragment_from_twws1_2pc;
  } proc;

  // RPPM objects
  struct rppms_t
  {
    real_ppm_t* felblade;

    // Havoc
    real_ppm_t* demonic_appetite;
  } rppm;

  // Shuffled proc objects
  struct shuffled_rngs_t
  {
  } shuffled_rng;

  // Special
  struct actives_t
  {
    // General
    heal_t* consume_soul_greater     = nullptr;
    heal_t* consume_soul_lesser      = nullptr;
    spell_t* immolation_aura         = nullptr;
    spell_t* immolation_aura_initial = nullptr;
    spell_t* collective_anguish      = nullptr;

    // Havoc
    spell_t* burning_wound                      = nullptr;
    attack_t* demon_blades                      = nullptr;
    spell_t* fel_barrage                        = nullptr;
    spell_t* fodder_to_the_flame_damage         = nullptr;
    spell_t* inner_demon                        = nullptr;
    spell_t* ragefire                           = nullptr;
    attack_t* relentless_onslaught              = nullptr;
    attack_t* relentless_onslaught_annihilation = nullptr;
    attack_t* throw_glaive_t31_proc             = nullptr;
    attack_t* throw_glaive_bd_throw             = nullptr;
    attack_t* throw_glaive_ds_throw             = nullptr;

    // Vengeance
    spell_t* infernal_armor     = nullptr;
    spell_t* retaliation        = nullptr;
    heal_t* frailty_heal        = nullptr;
    spell_t* fiery_brand_t30    = nullptr;
    spell_t* sigil_of_flame_t31 = nullptr;

    // Aldrachi Reaver
    attack_t* art_of_the_glaive = nullptr;
    attack_t* preemptive_strike = nullptr;
    attack_t* warblades_hunger  = nullptr;

    // Fel-scarred
    action_t* burning_blades = nullptr;
    action_t* demonsurge     = nullptr;
  } active;

  // Options
  struct demon_hunter_options_t
  {
    double initial_fury;
    // Override for target's hitbox size, relevant for Fel Rush and Vengeful Retreat. -1.0 uses default SimC value.
    double target_reach;
    // Relative directionality for movement events, 1.0 being directly away and 2.0 being perpendicular
    double movement_direction_factor;
    // Chance every second to get hit by the fodder demon
    double fodder_to_the_flame_initiative_chance;
    // Chance of souls to be incidentally picked up on any movement ability due to being in pickup range
    double soul_fragment_movement_consume_chance;
  } options;

  demon_hunter_t( sim_t* sim, util::string_view name, race_e r );

  // overridden player_t init functions
  stat_e convert_hybrid_stat( stat_e s ) const override;
  void copy_from( player_t* source ) override;
  action_t* create_action( util::string_view name, util::string_view options ) override;
  void create_buffs() override;
  std::unique_ptr<expr_t> create_expression( util::string_view ) override;
  void create_options() override;
  std::string create_profile( save_e ) override;
  void init_absorb_priority() override;
  void init_action_list() override;
  void init_base_stats() override;
  void init_procs() override;
  void init_uptimes() override;
  void init_resources( bool force ) override;
  void init_special_effects() override;
  void init_rng() override;
  void init_scaling() override;
  void init_spells() override;
  void init_items() override;
  void init_finished() override;
  bool validate_fight_style( fight_style_e style ) const override;
  void invalidate_cache( cache_e ) override;
  resource_e primary_resource() const override;
  role_e primary_role() const override;

  // custom demon_hunter_t init functions
private:
  void create_cooldowns();
  void create_gains();
  void create_benefits();

public:
  // Default consumables
  std::string default_potion() const override;
  std::string default_flask() const override;
  std::string default_food() const override;
  std::string default_rune() const override;
  std::string default_temporary_enchant() const override;

  // overridden player_t stat functions
  double composite_armor() const override;
  double composite_base_armor_multiplier() const override;
  double composite_armor_multiplier() const override;
  double composite_melee_haste() const override;
  double composite_spell_haste() const override;
  double composite_player_multiplier( school_e ) const override;
  double composite_player_critical_damage_multiplier( const action_state_t* ) const override;
  double matching_gear_multiplier( attribute_e attr ) const override;
  double stacking_movement_modifier() const override;

  // overridden player_t combat functions
  void assess_damage( school_e, result_amount_type, action_state_t* s ) override;
  void combat_begin() override;
  const demon_hunter_td_t* find_target_data( const player_t* target ) const override;
  demon_hunter_td_t* get_target_data( player_t* target ) const override;
  void interrupt() override;
  void regen( timespan_t periodicity ) override;
  double resource_gain( resource_e, double, gain_t* source = nullptr, action_t* action = nullptr ) override;
  double resource_gain( resource_e, double, double, gain_t* source = nullptr, action_t* action = nullptr );
  double resource_loss( resource_e, double, gain_t* source = nullptr, action_t* action = nullptr ) override;
  void recalculate_resource_max( resource_e, gain_t* source = nullptr ) override;
  void reset() override;
  void merge( player_t& other ) override;
  void datacollection_begin() override;
  void datacollection_end() override;
  void target_mitigation( school_e, result_amount_type, action_state_t* ) override;
  void apply_affecting_auras( action_t& action ) override;
  void analyze( sim_t& sim ) override;

  // custom demon_hunter_t functions
  const spelleffect_data_t* find_spelleffect( const spell_data_t* spell, effect_subtype_t subtype = A_MAX,
                                              int misc_value               = P_GENERIC,
                                              const spell_data_t* affected = spell_data_t::nil(),
                                              effect_type_t type           = E_APPLY_AURA );
  const spell_data_t* find_spell_override( const spell_data_t* base, const spell_data_t* passive );
  const spell_data_t* find_spell_override( const spell_data_t* base, std::vector<const spell_data_t*> passives );
  void set_out_of_range( timespan_t duration );
  void adjust_movement();
  double calculate_expected_max_health() const;
  unsigned consume_soul_fragments( soul_fragment = soul_fragment::ANY, bool heal = true,
                                   unsigned max = MAX_SOUL_FRAGMENTS );
  unsigned consume_nearby_soul_fragments( soul_fragment = soul_fragment::ANY );
  unsigned get_active_soul_fragments( soul_fragment = soul_fragment::ANY ) const;
  unsigned get_inactive_soul_fragments( soul_fragment = soul_fragment::ANY ) const;
  unsigned get_total_soul_fragments( soul_fragment = soul_fragment::ANY ) const;
  void activate_soul_fragment( soul_fragment_t* );
  void spawn_soul_fragment( soul_fragment, unsigned = 1, bool = false );
  void spawn_soul_fragment( soul_fragment, unsigned, player_t* target, bool = false );
  void trigger_demonic();
  void trigger_demonsurge( actions::demonsurge_ability );
  double get_target_reach() const;
  void parse_player_effects();

  // Secondary Action Tracking
private:
  std::vector<action_t*> background_actions;

public:
  template <typename T, typename... Ts>
  T* find_background_action( util::string_view n = {} );

  template <typename T, typename... Ts>
  T* get_background_action( util::string_view n, Ts&&... args );

  // Cooldown Tracking
  template <typename T_CONTAINER, typename T_DATA>
  T_CONTAINER* get_data_entry( util::string_view name, std::vector<T_DATA*>& entries );

private:
  target_specific_t<demon_hunter_td_t> _target_data;
};
}  // namespace demon_hunter
