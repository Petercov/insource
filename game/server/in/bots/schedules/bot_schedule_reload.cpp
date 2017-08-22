//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// Authors: 
// Iv�n Bravo Bravo (linkedin.com/in/ivanbravobravo), 2017

#include "cbase.h"
#include "bots\bot.h"

#include "in_utils.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//================================================================================
//================================================================================
BEGIN_SETUP_SCHEDULE( CReloadSchedule )
    ADD_TASK( BTASK_RELOAD, NULL )

    ADD_INTERRUPT( BCOND_HEAVY_DAMAGE )
    ADD_INTERRUPT( BCOND_EMPTY_PRIMARY_AMMO )
END_SCHEDULE()

//================================================================================
//================================================================================
float CurrentSchedule::GetDesire() const
{
	if ( GetDecision()->ShouldCover() )
		return BOT_DESIRE_NONE;

    if ( HasCondition(BCOND_EMPTY_PRIMARY_AMMO) )
        return BOT_DESIRE_NONE;

    if ( HasCondition(BCOND_EMPTY_CLIP1_AMMO) )
		return 0.81f;

    if ( HasCondition(BCOND_LOW_CLIP1_AMMO) && !HasCondition(BCOND_LOW_PRIMARY_AMMO) && GetBot()->IsIdle() )
        return 0.43f;

	return BOT_DESIRE_NONE;
}