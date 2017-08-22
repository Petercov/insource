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
BEGIN_SETUP_SCHEDULE( CCallBackupSchedule )
    ADD_TASK( BTASK_SAVE_POSITION,	 NULL )
	ADD_TASK( BTASK_RUN,	         NULL )
    ADD_TASK( BTASK_GET_FAR_COVER,	 1000.0f )
	ADD_TASK( BTASK_CROUCH,			 NULL )
	ADD_TASK( BTASK_HEAL,			 NULL )
	ADD_TASK( BTASK_RELOAD,			 NULL )
    ADD_TASK( BTASK_WAIT,			 RandomFloat(2.0f, 6.0f) )
	ADD_TASK( BTASK_CALL_FOR_BACKUP, NULL )
	ADD_TASK( BTASK_WAIT,			 RandomFloat(2.0f, 6.0f) )
	ADD_TASK( BTASK_RESTORE_POSITION, NULL )

    ADD_INTERRUPT( BCOND_DEJECTED )
END_SCHEDULE()