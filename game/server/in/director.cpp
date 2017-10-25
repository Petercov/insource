//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// Authors: 
// Iv�n Bravo Bravo (linkedin.com/in/ivanbravobravo), 2017

#include "cbase.h"
#include "director.h"
#include "director_manager.h"

#include "in_shareddefs.h"
#include "in_gamerules.h"
#include "in_player.h"
#include "players_system.h"
#include "in_utils.h"

#include "fmtstr.h"
#include "world.h"

#include "vscript_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

Director g_Director;
Director *TheDirector = &g_Director;

//================================================================================
// Comandos
//================================================================================

DECLARE_DEBUG_COMMAND( director_debug, "0", "" )

DECLARE_NOTIFY_COMMAND( director_adaptative_skill, "0", "" )
DECLARE_NOTIFY_COMMAND( director_max_childs, "30", "" )
DECLARE_NOTIFY_COMMAND( director_max_distance, "3000", "" )
DECLARE_NOTIFY_COMMAND( director_min_distance, "600", "" )
DECLARE_CHEAT_COMMAND( director_disable_music, "0", "" )

#define IS_ANGRY GetAngry() >= ANGRY_HIGH
#define IS_CRAZY GetAngry() >= ANGRY_CRAZY

//================================================================================
// Informaci�n para la maquina virtual
//================================================================================

BEGIN_ENT_SCRIPTDESC_ROOT( Director, SCRIPT_SINGLETON "TheDirector" )
	DEFINE_SCRIPTFUNC( SetPopulation, "" )
	//DEFINE_SCRIPTFUNC_NAMED( ScriptGetChildClass, "GetChildClass", "" )

	DEFINE_SCRIPTFUNC( Resume, "" )
	DEFINE_SCRIPTFUNC( Stop, "" )

	DEFINE_SCRIPTFUNC( GetPanicEventDelay, "" )
	DEFINE_SCRIPTFUNC( GetPanicEventHordes, "" )
	DEFINE_SCRIPTFUNC( StartPanic, "" )

	DEFINE_SCRIPTFUNC( SoundDesire, "" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptCanSpawnMinions, "CanSpawnMinions", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptCanSpawnMinionsByType, "CanSpawnMinionsByType", "" )

	DEFINE_SCRIPTFUNC( GetMaxDistance, "" )
	DEFINE_SCRIPTFUNC( GetMinDistance, "" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetMaxUnits, "GetMaxUnits", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetMaxUnitsByType, "GetMaxUnitsByType", "" )

	DEFINE_SCRIPTFUNC( Disclose, "" )
	DEFINE_SCRIPTFUNC( KillAll, "" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptSet, "Set", "" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetStatus, "GetStatus", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptIsStatus, "IsStatus", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetStatus, "SetStatus", "" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetPhase, "GetPhase", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptIsPhase, "IsPhase", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetPhase, "SetPhase", "" )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetAngry, "GetAngry", "" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetAngry, "SetAngry", "" )
END_SCRIPTDESC();

#undef VSCRIPT_CALL
#undef VSCRIPT_RESULT

#define VSCRIPT_CALL( methodname ) ScriptVariant_t _retVal; CallScriptFunction( #methodname, &_retVal )
#define VSCRIPT_RESULT( type, value ) _retVal.Get<type>() == value

#define HANDLED_BY_VSCRIPT( methodname ) ScriptVariant_t _retVal; \
	if ( CallScriptFunction(#methodname, &_retVal) ) { \
		if ( VSCRIPT_RESULT(bool, true) ) \
			return; \
	}

#define IF_NOT_HANDLED_BY_VSCRIPT(methodname) ScriptVariant_t _retVal; \
	if ( CallScriptFunction(#methodname, &_retVal) && VSCRIPT_RESULT(bool, true) ) {\
		return; \
    } else \

#define HANDLED_BY_VSCRIPT_VALUE( type, methodname, condition, value ) ScriptVariant_t _retVal; \
	if ( CallScriptFunction(#methodname, &_retVal) ) { \
		if ( !_retVal.IsNull() && _retVal.Get<type>() condition value ) \
			return _retVal.Get<type>(); \
	}

#define HANDLED_BY_VSCRIPT_VALUE_ARG1( type, methodname, arg1type, arg1value, condition, value ) ScriptVariant_t _retVal; \
	if ( CallScriptFunction<arg1type>(#methodname, &_retVal, arg1value) ) { \
		if ( !_retVal.IsNull() && _retVal.Get<type>() condition value ) \
			return _retVal.Get<type>(); \
	}

#define HANDLED_BY_VSCRIPT_RETURN( type, methodname ) ScriptVariant_t _retVal; \
	if ( CallScriptFunction(#methodname, &_retVal) ) { \
		return _retVal.Get<type>(); \
	}

#define HANDLED_BY_VSCRIPT_RETURN_ARG1( type, methodname, arg1type, arg1value ) ScriptVariant_t _retVal; \
	if ( CallScriptFunction<arg1type>(#methodname, &_retVal, arg1value) ) { \
		return _retVal.Get<type>(); \
	}

//================================================================================
//================================================================================
Director::Director() : CAutoGameSystemPerFrame( "Director" )
{

}

//================================================================================
//================================================================================
void Director::PostInit()
{
    Reset();
}

//================================================================================
//================================================================================
void Director::Shutdown()
{
}

//================================================================================
//================================================================================
void Director::LevelInitPreEntity()
{
    m_bDisabled = false;
}

//================================================================================
//================================================================================
void Director::LevelInitPostEntity()
{
    if ( !TheGameRules )
        return;

    if ( !TheGameRules->HasDirector() )
        return;

    CreateMusic();
    StartMap();
}

//================================================================================
//================================================================================
void Director::LevelShutdownPreEntity()
{
    if ( !TheGameRules )
        return;

    if ( !TheGameRules->HasDirector() )
        return;

    Stop();
}

//================================================================================
//================================================================================
void Director::LevelShutdownPostEntity()
{
}

//================================================================================
// Despu�s de que las entidades hayan pensado
//================================================================================
void Director::FrameUpdatePostEntityThink()
{
    if ( !TheGameRules )
        return;

    if ( !TheGameRules->HasDirector() )
        return;

    Think();
}

//================================================================================
//================================================================================
void Director::SetPopulation( const char *type )
{
    TheDirectorManager->SetPopulation( type );
}

//================================================================================
// Reanuda el procesamiento del Director
//================================================================================
void Director::Resume()
{
    m_bDisabled = false;
    VSCRIPT_CALL( Resume );
}

//================================================================================
// Para el procesamiento del Director
//================================================================================
void Director::Stop()
{
    KillAll( false );
    m_bDisabled = true;
    //VSCRIPT_CALL( Stop );
}

//================================================================================
//================================================================================
void Director::StartVirtualMachine()
{
    // No hay maquina virtual
    if ( !scriptmanager || !g_pScriptVM )
        return;

    // Registramos el Director
    //g_pScriptVM->RegisterInstance( TheDirector, "TheDirector" );

    // Configuraci�n predeterminada del Director
    KeyValues *config = new KeyValues( "DirectorConfig" );
    config->LoadFromFile( filesystem, "scripts/director_config.txt", NULL );

    // Registramos el arreglo para que los scripts puedan cambiar la configuraci�n
    CScriptKeyValues *pConfig = new CScriptKeyValues( config );
    g_pScriptVM->RegisterInstance( pConfig, "DirectorConfig" );

    //if ( m_ScriptScope.IsInitialized() )
    //return;

    // Creamos la identificaci�n
    if ( m_szScriptID == NULL_STRING ) {
        char *szName = (char *)stackalloc( 1024 );
        g_pScriptVM->GenerateUniqueKey( "TheDirector", szName, 1024 );
        m_szScriptID = AllocPooledString( szName );
    }

    // Registramos al Director y le establecemos la identificaci�n
    m_hScriptInstance = g_pScriptVM->RegisterInstance( GetScriptDesc(), this );
    g_pScriptVM->SetInstanceUniqeId( m_hScriptInstance, STRING( m_szScriptID ) );

    // Creamos el scope
    m_ScriptScope.Init( STRING( m_szScriptID ) );
    g_pScriptVM->SetValue( m_ScriptScope, "self", m_hScriptInstance );

    VScriptRunScript( "director_base.nut", m_ScriptScope, true );
    VScriptRunScript( UTIL_VarArgs( "director/%s.nut", gpGlobals->mapname ), m_ScriptScope );
}

//================================================================================
//================================================================================
bool Director::CallScriptFunction( const char * pFunctionName, ScriptVariant_t *pFunctionReturn )
{
    if ( !m_ScriptScope.IsInitialized() )
        return false;

    HSCRIPT hFunc = m_ScriptScope.LookupFunction( pFunctionName );

    if ( hFunc ) {
        m_ScriptScope.Call( hFunc, pFunctionReturn );
        m_ScriptScope.ReleaseFunction( hFunc );

        return true;
    }

    return false;
}

//================================================================================
//================================================================================
void Director::Reset()
{
    m_iStatus = STATUS_INVALID;
    m_iPhase = PHASE_INVALID;
    m_iAngry = ANGRY_INVALID;

    m_PhaseTimer.Invalidate();
    m_ThinkTimer.Invalidate();

    m_SustainTimer.Invalidate();
    m_PanicTimer.Invalidate();

    m_MapTimer.Invalidate();
    m_GameTimer.Invalidate();

    m_pEntity = NULL;

    m_bDisabled = false;
    m_iPanicEventHordesLeft = 0;

    TheDirectorManager->Init();
}

//================================================================================
//================================================================================
void Director::StartMap()
{
    if ( !m_GameTimer.HasStarted() ) {
        StartCampaign();
    }

    FindDirectorEntity();
    StartVirtualMachine();

    Set( STATUS_NORMAL, PHASE_RELAX, 5.0f );

    m_ThinkTimer.Start( 3.0f );
    m_SustainTimer.Invalidate();
    m_PanicTimer.Start( GetPanicEventDelay() );

    m_MapTimer.Start();
    m_iPanicEventHordesLeft = 0;

    TheDirectorManager->ResetMap();    
    VSCRIPT_CALL( StartMap );

    // TODO
    m_BackgroundMusicTimer.Start( RandomInt( 10, 20 ) );
    m_ChoirTimer.Start( 10 );
    m_HordeMusic = m_HordeMusicList[RandomInt( HORDE_DEVIDDLE, LAST_HORDE_MUSIC - 1 )];
}

//================================================================================
// Reinicia el Director para una nueva campa�a/partida
//================================================================================
void Director::StartCampaign()
{
    SetAngry( ANGRY_LOW );
    m_GameTimer.Start();
}

//================================================================================
//================================================================================
void Director::FindDirectorEntity()
{
    // Ya lo tenemos
    if ( m_pEntity )
        return;

    m_pEntity = (CInfoDirector *)gEntList.FindEntityByClassname( NULL, "info_director" );

    // No hay ning�n info_director
    if ( !m_pEntity ) {
        m_pEntity = (CInfoDirector *)CBaseEntity::CreateNoSpawn( "info_director", vec3_origin, vec3_angle );
        m_pEntity->SetName( MAKE_STRING( "@director" ) );
        DispatchSpawn( m_pEntity );
        m_pEntity->Activate();
    }

    Assert( m_pEntity );
}

//================================================================================
//================================================================================
void Director::Think()
{
    if ( engine->IsInEditMode() )
        return;

    if ( director_debug.GetBool() ) {
        DebugDisplay();
    }

    if ( !m_ThinkTimer.IsElapsed() || IsDisabled() )
        return;

    PreUpdate();
    Update();
    PostUpdate();

    m_ThinkTimer.Start( 0.3f );
}

//================================================================================
//================================================================================
void Director::PreUpdate()
{
    VSCRIPT_CALL( PreUpdate );

    TheDirectorManager->Scan();
    TheGameRules->Director_PreUpdate();
}

//================================================================================
// Actualiza el entorno, los hijos y el estado del director
//================================================================================
void Director::Update()
{
    VSCRIPT_CALL( Update );

    UpdateAngry();
    UpdatePhase();
    UpdatePanicEvent();

    m_MusicManager->Update();

    TheDirectorManager->Spawn();

    TheGameRules->Director_Update();
}

//================================================================================
// Actualiza el entorno del director despu�s de haber creado hijos
//================================================================================
void Director::PostUpdate()
{
    VSCRIPT_CALL( PostUpdate );

    // Attack!
    if ( IsOnPanicEvent() ) {
        TheDirector->Disclose();
    }
}

//================================================================================
// Actualiza la fase actual del Director
//================================================================================
void Director::UpdatePhase()
{
    HANDLED_BY_VSCRIPT( UpdatePhase );

    DirectorPhase phase = GetPhase();

    // Esta fase deber�a terminar
    if ( ShouldPhaseEnd( phase ) ) {
        OnPhaseEnd( phase );
    }
}

//================================================================================
// Actualiza el nivel del enojo del Director y la dificultad del juego
// TODO: M�s variables
//================================================================================
void Director::UpdateAngry()
{
    HANDLED_BY_VSCRIPT( UpdateAngry );

    // ANGRY_LOW = 1
    int angry = ANGRY_LOW;

    // ANGRY_MEDIUM, ANGRY_HIGH, ANGRY_CRAZY

    // M�s de 5 minutos en el mapa
    if ( m_MapTimer.GetElapsedTime() > (5 * 60) ) {
        // Los jugadores estan bien
        if ( ThePlayersSystem->GetStats() >= STATS_GOOD )
            ++angry;

        // Estan muy bien!
        if ( ThePlayersSystem->GetStats() >= STATS_EXCELENT )
            ++angry;

        // Ya he creado m�s de 500 hijos
        if ( TheDirectorManager->GetMinionsCreated() >= 500 )
            ++angry;
    }

    // Establecemos
    angry = clamp( angry, ANGRY_LOW, ANGRY_CRAZY );
    SetAngry( (DirectorAngry)angry );
}

//================================================================================
// Actualiza el sistema de p�nico
// Si es hora de un evento de p�nico decide cuantas hordas y cuando ser� el pr�ximo
//================================================================================
void Director::UpdatePanicEvent()
{
    HANDLED_BY_VSCRIPT( UpdatePanicEvent );

    if ( !ShouldStartPanicEvent() )
        return;

    StartPanic( GetPanicEventHordes() );
    m_PanicTimer.Start( GetPanicEventDelay() );
}

//================================================================================
// Devuelve si la fase indicada deber�a terminar
//================================================================================
bool Director::ShouldPhaseEnd( DirectorPhase phase )
{
    HANDLED_BY_VSCRIPT_RETURN_ARG1( bool, ShouldPhaseEnd, int, phase );

    switch ( phase ) {
        // Give players a break
        case PHASE_RELAX:
        {
            // Nothing
            break;
        }

        // Create enemies until players reach a high level of stress
        case PHASE_BUILD_UP:
        {
            if ( GetStress() >= GetMaxStress() )
                return true;

            break;
        }

        // Keep the stress level high for a while
        case PHASE_SUSTAIN:
        {
            if ( !IsOnPanicEvent() )
                return true;

            if ( m_SustainTimer.HasStarted() && m_SustainTimer.IsElapsed() )
                return true;

            if ( GetStress() >= 100.0f )
                return true;

            break;
        }

        // Wait until the enemies are eliminated and the level of stress goes down
        case PHASE_FADE:
        {
            if ( GetStress() <= GetMinStress() && TheDirectorManager->m_Minions[CHILD_TYPE_COMMON].alive <= 5 )
                return true;

            break;
        }

        default:
        {
            Assert(!"Invalid Phase");
            break;
        }
    }

    if ( m_PhaseTimer.HasStarted() && m_PhaseTimer.IsElapsed() )
        return true;

    return false;
}

//================================================================================
//================================================================================
void Director::OnPhaseStart( DirectorPhase phase )
{
    switch ( phase ) {
        case PHASE_RELAX:
        {
            if ( IsStatus( STATUS_PANIC ) ) {
                EndPanicHorde();
            }

            break;
        }

        case PHASE_SUSTAIN:
        {
            m_SustainTimer.Start( (3.0f * 60.0f) );
            break;
        }
    }
}

//================================================================================
// Una fase ha terminado
//================================================================================
void Director::OnPhaseEnd( DirectorPhase phase )
{
    m_PhaseTimer.Invalidate();

    switch ( phase ) {
        // The break is over, time to fight.
        case PHASE_RELAX:
        {
            SetPhase( PHASE_BUILD_UP );
            break;
        }

        // The level of stress is high or we have finished creating enemies.
        case PHASE_BUILD_UP:
        {
            if ( IsOnPanicEvent() ) {
                SetPhase( PHASE_SUSTAIN );
            }
            else {
                if ( GetDifficulty() >= SKILL_HARD ) {
                    SetPhase( PHASE_RELAX, 10.0f );
                }
                else {
                    SetPhase( PHASE_FADE );
                }
            }
            
            break;
        }

        // It's been a while since players are highly stressed or have reached an extreme stress level.
        case PHASE_SUSTAIN:
        {
            SetPhase( PHASE_FADE );
            break;
        }

        // The enemies have been eliminated and the level of stress is low, let us rest for a while.
        case PHASE_FADE:
        {
            SetPhase( PHASE_RELAX, 10.0f );
            break;
        }
    }
}

//================================================================================
// Devuelve el tiempo en segundos para el pr�ximo evento de p�nico
//================================================================================
float Director::GetPanicEventDelay()
{
    HANDLED_BY_VSCRIPT_VALUE( float, GetPanicEventDelay, >, 0.0f );

    // De 5 a 7 minutos
    float delay = (60.0f * RandomFloat( 5.0f, 7.0f ));

    // Medio o dificil: -1 minuto
    if ( GetDifficulty() == SKILL_MEDIUM ) {
        delay -= 60.0f;
    }

    // Dificil -1 minuto
    if ( GetDifficulty() >= SKILL_HARD ) {
        delay -= 60.0f;
    }

    return delay;
}

//================================================================================
// Devuelve el n�mero de hordas que deber�a tener el siguiente evento de p�nico
//================================================================================
int Director::GetPanicEventHordes()
{
    HANDLED_BY_VSCRIPT_VALUE( int, GetPanicEventHordes, >, 0 );

    // N�mero de hordas
    int hordes = 1;

    // Estamos enojados
    if ( GetDifficulty() >= SKILL_HARD ) {
        hordes += 1;
    }

    // Estamos furiosos!
    if ( GetDifficulty() >= SKILL_VERY_HARD ) {
        hordes += 2;
    }

    return hordes;
}

//================================================================================
//================================================================================
bool Director::ShouldStartPanicEvent()
{
    HANDLED_BY_VSCRIPT_RETURN( bool, ShouldStartPanicEvent );

    if ( !IsStatus( STATUS_NORMAL ) )
        return false;

    if ( !m_PanicTimer.HasStarted() || !m_PanicTimer.IsElapsed() )
        return false;

    return true;
}

//================================================================================
// Empieza un evento de p�nico con el n�mero de hordas especificadas
//================================================================================
void Director::StartPanic( int hordes )
{
    // Hacemos espacio para los nuevos minions
    KillAll( true );

    // P�nico
    SetStatus( STATUS_PANIC );
    m_iPanicEventHordesLeft = hordes;

    OnPanicStart();
}

//================================================================================
//================================================================================
void Director::EndPanicHorde()
{
    if ( !IsStatus( STATUS_PANIC ) )
        return;

    if ( m_iPanicEventHordesLeft == INFINITE )
        return;

    int hordeNumber = m_iPanicEventHordesLeft;
    int hordesLeft = (m_iPanicEventHordesLeft - 1);

    OnPanicHordeEnd( hordeNumber, hordesLeft );

    --m_iPanicEventHordesLeft;

    if ( m_iPanicEventHordesLeft == 0 ) {
        OnPanicEnd();
    }
}

//================================================================================
// Un evento de p�nico ha comenzado
//================================================================================
void Director::OnPanicStart()
{
    VSCRIPT_CALL( OnPanicStart );

    if ( director_debug.GetBool() )
        DevMsg( "[Director] Un evento de panico ha comenzado.\n" );
}

//================================================================================
// Una horda de p�nico ha terminado
//================================================================================
void Director::OnPanicHordeEnd( int hordeNumber, int hordesLeft )
{
    // TODO: VSCRIPT_CALL 

    if ( director_debug.GetBool() )
        DevMsg( "[Director] Una horda ha terminado #%i - Faltan: %i.\n", hordeNumber, hordesLeft );
}

//================================================================================
// Un evento de p�nico ha terminado
//================================================================================
void Director::OnPanicEnd()
{
    HANDLED_BY_VSCRIPT( OnPanicEnd );

    // Estado normal
    SetStatus( STATUS_NORMAL );

    if ( director_debug.GetBool() )
        DevMsg( "[Director] Un evento de panico ha terminado.\n" );
}

//================================================================================
// Imprime toda la informaci�n posible del Director
//================================================================================
void Director::DebugDisplay()
{
    m_iDebugLine = 0;
    CFmtStr msg;

    float stress = GetStress();
    float maxStress = GetMaxStress();
    float minStress = GetMinStress();

    DebugScreenText( "TheDirector" );
    DebugScreenText( "------------------------------------------" );

    if ( IsDisabled() ) {
        DebugScreenText( "DISABLED!" );
    }

    DebugScreenText( "" );
    DebugScreenText( "Map Time: %.2f", m_MapTimer.GetElapsedTime() );
    DebugScreenText( "Game Time: %.2f", m_GameTimer.GetElapsedTime() );
    
    // Estado
    DebugScreenText( "" );
    DebugScreenText( "Status: %s", g_DirectorStatus[m_iStatus] );

    if ( m_PhaseTimer.HasStarted() ) {
        DebugScreenText( "Phase: %s (%.2f)", g_DirectorPhase[m_iPhase], m_PhaseTimer.GetRemainingTime() );
    }
    else {
        DebugScreenText( "Phase: %s", g_DirectorPhase[m_iPhase] );
    }
    

    if ( IsStatus( STATUS_PANIC ) ) {
        DebugScreenText( "  Hordes: %i", m_iPanicEventHordesLeft );
    }
    
    if ( IsPhase( PHASE_SUSTAIN ) ) {
        DebugScreenText( "  Time Left: %.2f", m_SustainTimer.GetRemainingTime() );
        
        if ( stress <= minStress ) {
            DebugScreenText( "  Unable to sustain! (%.2f %)", stress );
        }
        else if ( stress >= maxStress ) {
            DebugScreenText( "  Maintaining maximum level (%.2f %)", stress );
        }
    }

    if ( IsPhase( PHASE_BUILD_UP ) ) {
        DebugScreenText( "  Stress: %.2f / %.2f", stress, maxStress );
    }

    DebugScreenText( "" );
    DebugScreenText( "Angry: %s", g_DirectorAngry[m_iAngry] );
    DebugScreenText( "Panic: %.2fs", m_PanicTimer.GetRemainingTime() );
    DebugScreenText( "Stress: %.2f", GetStress() );

    DebugScreenText( "" );
    DebugScreenText( "------------------------------------------" );
    DebugScreenText( "" );

    FOR_EACH_MINION_TYPE( it )
    {
        int population = TheDirectorManager->GetPopulation( (MinionType)it );

        if ( population == 0 )
            continue;

        DebugScreenText( "%s", g_MinionTypes[it] );
        DebugScreenText( "---------------------------" );
        DebugScreenText( "Minions: %i / %i", TheDirectorManager->m_Minions[it].alive, GetMaxUnits( (MinionType)it ) );
        DebugScreenText( "Population: %i", population );
        DebugScreenText( "Too close: %i", TheDirectorManager->m_Minions[it].tooClose );
        DebugScreenText( "Created: %i", TheDirectorManager->m_Minions[it].created );
        DebugScreenText( "" );
    }

    /*DebugScreenText( "" );
    DebugScreenText( "------------------------------------------" );
    DebugScreenText( "" );

    FOR_EACH_VEC( TheDirectorManager->m_PopulationList, pt )
    {
        CMinionInfo *info = TheDirectorManager->m_PopulationList[pt];
        Assert( info );

        DebugScreenText( "%s", info->unit );
        DebugScreenText( "---------------------------" );
        if ( info->maxUnits > 0 )
            DebugScreenText( "Units: %i / %i", info->alive, info->maxUnits );
        else
            DebugScreenText( "Units: %i", info->alive );
        DebugScreenText( "Spawn Chance: %i %", info->spawnChance );
        DebugScreenText( "Type: %s", g_MinionTypes[info->type] );
        DebugScreenText( "Created: %i", info->created );
        DebugScreenText( "Next Spawn: %.2f", info->nextSpawn.GetRemainingTime() );
        DebugScreenText( "" );

    }*/

    DebugScreenText( "" );
    DebugScreenText( "------------------------------------------" );
    DebugScreenText( "" );

    DebugScreenText( "" );
    DebugScreenText( "Background Music: %.2f", m_BackgroundMusicTimer.GetRemainingTime() );
    DebugScreenText( "Choir Sound: %.2f", m_ChoirTimer.GetRemainingTime() );

    //
    DebugScreenText( "" );
    DebugScreenText( "Administrator" );
    DebugScreenText( "------------------------------------------" );

    DebugScreenText( "Areas: %i", TheDirectorManager->m_CandidateAreas.Count() );
    DebugScreenText( "Nodes: %i", TheDirectorManager->m_CandidateNodes.Count() );

    //
    DebugScreenText( "" );
    DebugScreenText( "Players" );
    DebugScreenText( "------------------------------------------" );
    DebugScreenText( "" );

    DebugScreenText( "Alive: %i/%i", ThePlayersSystem->GetAlive(), ThePlayersSystem->GetTotal() );
    DebugScreenText( "Health: %i", ThePlayersSystem->GetHealth() );
    DebugScreenText( "Stress: %.2f", ThePlayersSystem->GetStress() );
    DebugScreenText( "Dejected: %i", ThePlayersSystem->GetDejected() );
    DebugScreenText( "Weapon Stats: %s", g_StatsNames[ThePlayersSystem->m_WeaponsStats] );
    DebugScreenText( "Stats: %s", g_StatsNames[ThePlayersSystem->m_PlayerStats] );

    // Informaci�n
    /*DebugScreenText( "" );
    DebugScreenText( "Information" );
    DebugScreenText( "------------------------------------------" );
    DebugScreenText( "" );

    DebugScreenText( "realtime: %2.f", gpGlobals->realtime );
    DebugScreenText( "framecount: %i", gpGlobals->framecount );
    DebugScreenText( "absoluteframetime: %2.f", gpGlobals->absoluteframetime );
    DebugScreenText( "curtime: %2.f", gpGlobals->curtime );
    DebugScreenText( "frametime: %.2f", gpGlobals->frametime );
    DebugScreenText( "maxClients: %i", gpGlobals->maxClients );
    DebugScreenText( "tickcount: %i", gpGlobals->tickcount );*/
}

//================================================================================
//================================================================================
void Director::DebugScreenText( const char *pText, ... )
{
    //if ( line < 0 )
    int line = m_iDebugLine;

    char str[4096];
    va_list marker;
    va_start( marker, pText );
    Q_vsnprintf( str, sizeof( str ), pText, marker );
    va_end( marker );

    engine->Con_NPrintf( line, "%s\n", str );
    ++m_iDebugLine;
}

//================================================================================
//================================================================================
void Director::CreateMusic()
{
    if ( m_MusicManager ) {
        delete m_MusicManager;
    }

    m_MusicManager = new CSoundManager();
    TheGameRules->Director_CreateMusic( m_MusicManager );
}

//================================================================================
//================================================================================
float Director::SoundDesire( const char *soundname, int channel )
{
    if ( director_disable_music.GetBool() )
        return 0.0f;

    // CHANNEL_1
    if ( FStrEq( soundname, "Music.Director.Boss" ) ) {
        if ( GetStatus() == STATUS_BOSS )
            return 1.0f;
    }

    if ( FStrEq( soundname, "Music.Director.BigBoss" ) ) {
    }

    // CHANNEL_2
    if ( FStrEq( soundname, "Music.Director.MiniFinale" ) ) {
    }

    if ( FStrEq( soundname, "Music.Director.Finale" ) ) {
        if ( GetStatus() == STATUS_FINALE )
            return 1.0f;
    }

    // CHANNEL_3
    if ( GetStatus() == STATUS_NORMAL && m_BackgroundMusicTimer.IsElapsed() ) {
        if ( FStrEq( soundname, "Music.Director.Background.LowAngry" ) ) {
            if ( GetAngry() == ANGRY_LOW ) {
                return 1.0f;
            }
        }

        if ( FStrEq( soundname, "Music.Director.Background.MediumAngry" ) ) {
            if ( GetAngry() == ANGRY_MEDIUM ) {
                return 1.0f;
            }
        }

        if ( FStrEq( soundname, "Music.Director.Background.HighAngry" ) ) {
            if ( GetAngry() == ANGRY_HIGH ) {
                return 1.0f;
            }
        }

        if ( FStrEq( soundname, "Music.Director.Background.CrazyAngry" ) ) {
            if ( GetAngry() == ANGRY_CRAZY ) {
                return 1.0f;
            }
        }
    }

    // CHANNEL_ANY
    if ( FStrEq( soundname, "Music.Director.Horde" ) ) {
        if ( GetStatus() == STATUS_PANIC )
            return 2.0f;
    }
    if ( FStrEq( soundname, "Music.Director.HordeSlayer" ) ) {
        if ( GetStatus() == STATUS_PANIC ) {
            int childs = TheDirectorManager->m_Minions[CHILD_TYPE_COMMON].alive;
            float volume = (0.2f * childs);

            return clamp( volume, 0.1f, 1.0f );
        }
    }
    if ( m_HordeMusic ) {
        if ( FStrEq( soundname, m_HordeMusic->GetSoundName() ) ) {
            if ( GetStatus() == STATUS_PANIC ) {
                if ( m_iPanicEventHordesLeft == INFINITE && TheDirectorManager->m_Minions[CHILD_TYPE_COMMON].alive < 3 )
                    return 0.1f;
                else
                    return 2.0f;
            }
        }
    }

    if ( FStrEq( soundname, "Music.Director.MobRules" ) ) {

    }

    if ( FStrEq( soundname, "Music.Director.FinalNail" ) ) {

    }

    if ( FStrEq( soundname, "Music.Director.Preparation" ) ) {

    }

    if ( FStrEq( soundname, "Music.Director.Gameover" ) ) {
        if ( GetStatus() == STATUS_GAMEOVER )
            return 2.0f;
    }

    if ( FStrEq( soundname, "Music.Director.InfectedChoir" ) ) {
        if ( GetStatus() == STATUS_NORMAL && m_ChoirTimer.IsElapsed() && TheDirectorManager->m_Minions[CHILD_TYPE_COMMON].tooClose > 6 )
            return 2.0f;
    }

    return 0.0f;
}

//================================================================================
//================================================================================
void Director::OnSoundPlay( const char *soundname )
{
    if ( strstr( soundname, "Music.Director.Background" ) != NULL ) {
        m_BackgroundMusicTimer.Start( RandomInt( 60, 300 ) );
    }

    if ( FStrEq( soundname, "Music.Director.InfectedChoir" ) ) {
        m_ChoirTimer.Start( RandomInt( 30, 60 ) );
    }
}

//================================================================================
// Devuelve si se pueden crear m�s minions
//================================================================================
bool Director::CanSpawnMinions()
{
    // El motor se esta quedando sin espacios
    if ( Utils::RunOutEntityLimit( DIRECTOR_TOLERANCE_ENTITIES ) ) {
        AssertOnce( !"RunOutEntityLimit!!" );
        Warning( "Utils::RunOutEntityLimit! \n" );
        return false;
    }

    HANDLED_BY_VSCRIPT_RETURN( bool, CanSpawnMinions );

    if ( IsPhase( PHASE_RELAX ) || IsPhase( PHASE_FADE ) )
        return false;

    if ( TheDirectorManager->m_CandidateAreas.Count() == 0 && TheDirectorManager->m_CandidateNodes.Count() == 0 )
        return false;

    if ( IsPhase( PHASE_SUSTAIN ) ) {
        if ( GetStress() >= GetMaxStress() )
            return false;
    }

    return true;
}

//================================================================================
// Devuelve si se pueden crear minions del tipo especificado
//================================================================================
bool Director::CanSpawnMinions( MinionType type )
{
    if ( !CanSpawnMinions() )
        return false;

    if ( TheDirectorManager->GetPopulation( type ) == 0 )
        return false;

    int alive = TheDirectorManager->m_Minions[type].alive;
    int max = GetMaxUnits( type );

    if ( alive >= max )
        return false;

    HANDLED_BY_VSCRIPT_RETURN_ARG1( bool, CanSpawnMinionsByType, int, (int)type );
    return true;
}

//================================================================================
// Devuelve si se puede crear el minion especificado
//================================================================================
bool Director::CanSpawnMinion( CMinionInfo * info )
{
    if ( info->spawnChance <= 0 )
        return false;

    if ( !info->nextSpawn.IsElapsed() )
        return false;

    if ( info->maxUnits > 0 ) {
        if ( info->alive >= info->maxUnits )
            return false;
    }

    return true;
}

//================================================================================
//================================================================================
int Director::GetDifficulty()
{
    int difficulty = GetGameDifficulty();

    switch ( GetAngry() ) {
        case ANGRY_LOW:
        {
            difficulty -= 2;
            break;
        }

        case ANGRY_MEDIUM:
        {
            difficulty -= 1;
            break;
        }

        case ANGRY_HIGH:
        {
            difficulty += 1;
            break;
        }

        case ANGRY_CRAZY:
        {
            difficulty += 2;
            break;
        }
    }

    difficulty = clamp( difficulty, SKILL_EASIEST, SKILL_HARDEST );
    return difficulty;
}

//================================================================================
// Devuelve la distancia m�xima de los jugadores en la que se puede crear un hijo
//================================================================================
float Director::GetMaxDistance()
{
    HANDLED_BY_VSCRIPT_VALUE( float, GetMaxDistance, >, 300.0f );

    float distance = director_max_distance.GetFloat();

    if ( IsOnPanicEvent() ) {
        distance -= 200.0f;
    }

    return MAX( distance, 300.0f );
}

//================================================================================
// Devuelve la distancia minima de los jugadores en la que se puede crear un hijo
//================================================================================
float Director::GetMinDistance()
{
    HANDLED_BY_VSCRIPT_VALUE( float, GetMinDistance, >, 0.0f );

    float distance = director_min_distance.GetFloat();
    return MAX( distance, 0.0f );
}

//================================================================================
//================================================================================
float Director::GetMinStress()
{
    switch ( GetPhase() ) {
        // Minimum level that we must maintain during SUSTAIN
        case PHASE_SUSTAIN:
        {
            float level = 70.0f;

            if ( GetDifficulty() <= SKILL_MEDIUM ) {
                level = 50.0f;
            }

            if ( GetDifficulty() <= SKILL_EASY ) {
                level = 40.0f;
            }

            return level;
            break;
        }

        // If we reach this level and the minions have been eliminated we will move to RELAX
        case PHASE_FADE:
        {
            return 30.0f;
            break;
        }
    }

    return 0.0f;
}

//================================================================================
//================================================================================
float Director::GetMaxStress()
{
    switch ( GetPhase() ) {
        // If you pass from this level we move to SUSTAIN and try to keep it between minimum and maximum
        case PHASE_BUILD_UP:
        {
            float level = 70.0f;

            if ( GetDifficulty() <= SKILL_MEDIUM ) {
                level = 50.0f;
            }

            if ( GetDifficulty() <= SKILL_EASY ) {
                level = 40.0f;
            }

            return level;
            break;
        }

        // If we reach this level we will stop creating minions
        case PHASE_SUSTAIN:
        {
            float level = 90.0f;

            if ( GetDifficulty() <= SKILL_MEDIUM ) {
                level = 70.0f;
            }

            if ( GetDifficulty() <= SKILL_EASY ) {
                level = 60.0f;
            }

            return level;
            break;
        }
    }

    return 100.0f;
}

//================================================================================
//================================================================================
float Director::GetStress()
{
    return ThePlayersSystem->GetStress();
}

//================================================================================
//================================================================================
int Director::GetMaxUnits()
{
    HANDLED_BY_VSCRIPT_VALUE( int, GetMaxUnits, >= , 0 );
    return director_max_childs.GetInt();
}

//================================================================================
//================================================================================
int Director::GetMaxUnits( MinionType type )
{
    HANDLED_BY_VSCRIPT_VALUE_ARG1( int, GetMaxUnitsByType, int, (int)type, >= , 0 );
    return GetMaxUnits();
}

//================================================================================
// Reporta a todos los hijos la posici�n de los jugadores
//================================================================================
void Director::Disclose()
{
    CBaseEntity *pMinion = NULL;

    do {
        pMinion = gEntList.FindEntityByName( pMinion, "director_*" );

        if ( !pMinion || !pMinion->IsAlive() )
            continue;

        TheDirectorManager->ReportEnemy( pMinion );
    }
    while ( pMinion );
}

//================================================================================
// Mata a todos los hijos
//================================================================================
void Director::KillAll( bool onlyNoVisible )
{
    CBaseEntity *pMinion = NULL;

    do {
        pMinion = gEntList.FindEntityByName( pMinion, "director_*" );

        if ( !pMinion || !pMinion->IsAlive() )
            continue;

        if ( onlyNoVisible && ThePlayersSystem->IsEyesVisible( pMinion ) )
            continue;

        TheDirectorManager->Kill( pMinion, "Kill All" );
    }
    while ( pMinion );
}

//================================================================================
// Establece el estado y la fase del Director
//================================================================================
void Director::Set( DirectorStatus status, DirectorPhase phase, float duration )
{
    SetStatus( status );
    SetPhase( phase, duration );
}

//================================================================================
// Establece el estado y la fase del Director
//================================================================================
void Director::ScriptSet( int status, int phase, float duration )
{
    Set( (DirectorStatus)status, (DirectorPhase)phase, duration );
}

//================================================================================
//================================================================================
void Director::SetStatus( DirectorStatus status )
{
    m_iStatus = status;
}

//================================================================================
//================================================================================
void Director::SetPhase( DirectorPhase phase, float duration )
{
    m_iPhase = phase;

    if ( duration > 0.0f ) {
        m_PhaseTimer.Start( duration );
    }
    else {
        m_PhaseTimer.Invalidate();
    }

    OnPhaseStart( phase );
}

//================================================================================
//================================================================================
void Director::SetAngry( DirectorAngry angry )
{
    m_iAngry = angry;

    // Establecemos la dificultad del juego
    if ( TheGameRules->Director_AdaptativeSkill() ) {
        ConVarRef sv_difficulty( "sv_difficulty" );
        sv_difficulty.SetValue( (int)angry );
    }
}

void CC_Stop()
{
    if ( !TheDirector )
        return;

    TheDirector->Stop();
}

void CC_Resume()
{
    if ( !TheDirector )
        return;

    TheDirector->Resume();
}

void CC_ForceNormal()
{
    if ( !TheDirector )
        return;

    TheDirector->SetStatus( STATUS_NORMAL );
}

void CC_ForceBoss()
{
    if ( !TheDirector )
        return;

    //Director->m_bBossPendient = true;
}

void CC_ForcePanic()
{
    if ( !TheDirector )
        return;

    TheDirector->StartPanic( 5 );
}

void CC_ForcePanicInfinite()
{
    if ( !TheDirector )
        return;

    TheDirector->StartPanic( INFINITE );
}

void CC_ForceFinale()
{
    if ( !TheDirector )
        return;

    TheDirector->SetStatus( STATUS_FINALE );
}

void CC_ForceKill()
{
    if ( !TheDirector )
        return;

    TheDirector->KillAll( false );
}

static ConCommand director_stop( "director_stop", CC_Stop );
static ConCommand director_resume( "director_resume", CC_Resume );
static ConCommand director_force_normal( "director_force_normal", CC_ForceNormal );
//static ConCommand director_force_boss( "director_force_boss", CC_ForceBoss );
static ConCommand director_force_panic( "director_force_panic", CC_ForcePanic );
static ConCommand director_force_panic_infinite( "director_force_panic_infinite", CC_ForcePanicInfinite );
static ConCommand director_force_finale( "director_force_finale", CC_ForceFinale );
static ConCommand director_kill_minions( "director_kill_minions", CC_ForceKill );