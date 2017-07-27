//==== Woots 2016. http://creativecommons.org/licenses/by/2.5/mx/ ===========//

#include "cbase.h"
#include "bot.h"

#include "in_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//================================================================================
//================================================================================
BEGIN_SETUP_SCHEDULE( CInvestigateLocationSchedule )
    ADD_TASK( BTASK_SAVE_LOCATION,	  NULL )
	ADD_TASK( BTASK_RUN,              NULL)
	ADD_TASK( BTASK_MOVE_DESTINATION,  m_vecLocation )
	ADD_TASK( BTASK_WAIT,			  RandomFloat(3.0f, 6.0f) ) // TODO
	ADD_TASK( BTASK_RESTORE_LOCATION, NULL )

    ADD_INTERRUPT( BCOND_NEW_ENEMY )
    ADD_INTERRUPT( BCOND_SEE_ENEMY )
	ADD_INTERRUPT( BCOND_SEE_FEAR )
    ADD_INTERRUPT( BCOND_LIGHT_DAMAGE )
    ADD_INTERRUPT( BCOND_HEAVY_DAMAGE )
    ADD_INTERRUPT( BCOND_LOW_HEALTH )
    ADD_INTERRUPT( BCOND_DEJECTED )
END_SCHEDULE()

//================================================================================
//================================================================================
float CurrentSchedule::GetDesire()
{
	return BOT_DESIRE_NONE;
}