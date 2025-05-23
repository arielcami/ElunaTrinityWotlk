/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "ScriptedCreature.h"
#include "the_black_morass.h"

enum AeonusTexts
{
    SAY_ENTER           = 0,
    SAY_AGGRO           = 1,
    SAY_BANISH          = 2,
    SAY_SLAY            = 3,
    SAY_DEATH           = 4,
    EMOTE_FRENZY        = 5
};

enum AeonusSpells
{
    SPELL_CLEAVE        = 40504,
    SPELL_TIME_STOP     = 31422,
    SPELL_ENRAGE        = 37605,
    SPELL_SAND_BREATH   = 31473
};

enum AeonusEvents
{
    EVENT_SANDBREATH    = 1,
    EVENT_TIMESTOP,
    EVENT_FRENZY
};

// 17881 - Aeonus
struct boss_aeonus : public BossAI
{
    boss_aeonus(Creature* creature) : BossAI(creature, TYPE_AEONUS) { }

    void Reset() override { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.ScheduleEvent(EVENT_SANDBREATH, 15s, 30s);
        events.ScheduleEvent(EVENT_TIMESTOP, 10s, 15s);
        events.ScheduleEvent(EVENT_FRENZY, 30s, 45s);

        Talk(SAY_AGGRO);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        //Despawn Time Keeper
        if (who->GetTypeId() == TYPEID_UNIT && who->GetEntry() == NPC_TIME_KEEPER)
        {
            if (me->IsWithinDistInMap(who, 20.0f))
            {
                Talk(SAY_BANISH);
                Unit::DealDamage(me, who, who->GetHealth(), nullptr, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, nullptr, false);
            }
        }

        ScriptedAI::MoveInLineOfSight(who);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);

        instance->SetData(TYPE_RIFT, DONE);
        instance->SetData(TYPE_MEDIVH, DONE); // FIXME: later should be removed
    }

    void KilledUnit(Unit* who) override
    {
        if (who->GetTypeId() == TYPEID_PLAYER)
            Talk(SAY_SLAY);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_SANDBREATH:
                    DoCastVictim(SPELL_SAND_BREATH);
                    events.Repeat(15s, 25s);
                    break;
                case EVENT_TIMESTOP:
                    DoCastSelf(SPELL_TIME_STOP);
                    events.Repeat(20s, 35s);
                    break;
                case EVENT_FRENZY:
                    Talk(EMOTE_FRENZY);
                    DoCastSelf(SPELL_ENRAGE);
                    events.Repeat(20s, 35s);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }

        DoMeleeAttackIfReady();
    }
};

void AddSC_boss_aeonus()
{
    RegisterBlackMorassCreatureAI(boss_aeonus);
}
