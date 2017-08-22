//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// Authors: 
// Iv�n Bravo Bravo (linkedin.com/in/ivanbravobravo), 2017

#include "cbase.h"
#include "bot.h"

#include "in_utils.h"

#ifdef INSOURCE_DLL
#include "in_gamerules.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar bot_primary_attack;

//================================================================================
// Sets a condition
//================================================================================
void CBot::SetCondition( BCOND condition )
{
    m_nConditions.Set( condition );
}

//================================================================================
// Forgets a condition
//================================================================================
void CBot::ClearCondition( BCOND condition )
{
    m_nConditions.Clear( condition );
}

//================================================================================
// Returns if the bot is in a condition
//================================================================================
bool CBot::HasCondition( BCOND condition ) const
{
    return m_nConditions.IsBitSet( condition );
}

//================================================================================
// Add a component to the list
//================================================================================
void CBot::AddComponent( IBotComponent *pComponent )
{
    if ( pComponent->IsSchedule() ) {
        IBotSchedule *pSchedule = dynamic_cast<IBotSchedule *>(pComponent);
        Assert( pSchedule );

        if ( pSchedule ) {
            m_nSchedules.InsertOrReplace( pSchedule->GetID(), pSchedule );
        }
    }
    else {
        m_nComponents.InsertOrReplace( pComponent->GetID(), pComponent );
    }
}

//================================================================================
// Create the components that the bot will have
//================================================================================
void CBot::SetUpComponents()
{
    // Basic components
    // Each custom A.I. decide which ones to remove and add.
    ADD_COMPONENT( CBotVision );
    ADD_COMPONENT( CBotFollow );
    ADD_COMPONENT( CBotLocomotion );
    ADD_COMPONENT( CBotMemory );
    ADD_COMPONENT( CBotAttack );
    ADD_COMPONENT( CBotDecision ); // This component is mandatory!
}

//================================================================================
// Create the schedules that the bot will have
//================================================================================
void CBot::SetUpSchedules()
{
    // Basic schedules
    // Each custom A.I. decide which ones to remove and add.
    ADD_COMPONENT( CInvestigateSoundSchedule );
    //ADD_COMPONENT( CInvestigateLocationSchedule );
    ADD_COMPONENT( CHuntEnemySchedule );
    ADD_COMPONENT( CReloadSchedule );
    ADD_COMPONENT( CCoverSchedule );
    ADD_COMPONENT( CHideSchedule );
    ADD_COMPONENT( CChangeWeaponSchedule );
    ADD_COMPONENT( CHideAndHealSchedule );
    ADD_COMPONENT( CHideAndReloadSchedule );
    ADD_COMPONENT( CHelpDejectedFriendSchedule );
    ADD_COMPONENT( CMoveAsideSchedule );
    ADD_COMPONENT( CCallBackupSchedule );
    ADD_COMPONENT( CDefendSpawnSchedule );
}

//================================================================================
// Returns the [IBotSchedule] registered for this ID
//================================================================================
IBotSchedule *CBot::GetSchedule( int schedule )
{
    int index = m_nSchedules.Find( schedule );

    if ( m_nSchedules.IsValidIndex( index ) ) {
        return m_nSchedules.Element( index );
    }

    return NULL;
}

//================================================================================
// Returns the active schedule ID.
//================================================================================
int CBot::GetActiveScheduleID()
{
    if ( !GetActiveSchedule() ) {
        return SCHEDULE_NONE;
    }

    return GetActiveSchedule()->GetID();
}

//================================================================================
// Returns the ideal schedule for the bot (With more desire)
//================================================================================
int CBot::SelectIdealSchedule()
{
    if ( !GetHost()->IsAlive() ) {
        return SCHEDULE_NONE;
    }

    float desire = BOT_DESIRE_NONE;
    IBotSchedule *pIdeal = GetActiveSchedule();

    // We have an active schedule
    // We take her initial desire level
    if ( pIdeal ) {
        desire = pIdeal->GetInternalDesire();
    }

    FOR_EACH_MAP( m_nSchedules, it )
    {
        IBotSchedule *pSchedule = m_nSchedules[it];

        // This schedule has a greater desire!
        if ( pSchedule->GetInternalDesire() > desire ) {
            pIdeal = pSchedule;
            desire = pSchedule->GetDesire();
        }
    }

    if ( pIdeal && desire > BOT_DESIRE_NONE ) {
        return pIdeal->GetID();
    }

    return SCHEDULE_NONE;
}

//================================================================================
// Updates the current schedule
//================================================================================
void CBot::UpdateSchedule()
{
    VPROF_BUDGET( "UpdateSchedule", VPROF_BUDGETGROUP_BOTS );

    // Maybe an custom A.I. want to change a schedule.
    int idealSchedule = TranslateSchedule( SelectIdealSchedule() );

    if ( idealSchedule == SCHEDULE_NONE ) {
        if ( GetActiveSchedule() ) {
            GetActiveSchedule()->Finish();
            m_nActiveSchedule = NULL;
        }
        
        return;
    }

    IBotSchedule *pSchedule = GetSchedule( idealSchedule );
    AssertMsg( pSchedule, "GetSchedule == NULL" );

    if ( !pSchedule ) {
        return;
    }

    if ( GetActiveSchedule() ) {
        if ( GetActiveSchedule() == pSchedule ) {
            GetActiveSchedule()->Update();
            return;
        }
        else {
            GetActiveSchedule()->Finish();
        }
    }

    m_nActiveSchedule = pSchedule;
    m_nActiveSchedule->Start();
    m_nActiveSchedule->Update();
}

//================================================================================
// Mark a task as complete
// TODO: Real life scenario where this is used?
//================================================================================
void CBot::TaskComplete() 
{
	if ( !GetActiveSchedule() )
		return;

	GetActiveSchedule()->TaskComplete();
}

//================================================================================
// Mark as failed a schedule
// TODO: Real life scenario where this is used?
//================================================================================
void CBot::TaskFail( const char *pWhy ) 
{
	if ( !GetActiveSchedule() )
		return;

	GetActiveSchedule()->Fail( pWhy );
}


//================================================================================
// Gets new conditions from environment and statistics
// Conditions that do not require information about components (vision/smell/hearing)
//================================================================================
void CBot::SelectPreConditions()
{
    VPROF_BUDGET("SelectConditions", VPROF_BUDGETGROUP_BOTS);

    // TODO: Use them
    ClearCondition( BCOND_TASK_FAILED );
    ClearCondition( BCOND_TASK_DONE );
    ClearCondition( BCOND_SCHEDULE_FAILED );
    ClearCondition( BCOND_SCHEDULE_DONE );

    SelectHealthConditions();
    SelectWeaponConditions();
}

//================================================================================
// Gets new conditions from environment and statistics
// Conditions that require information about components (vision/smell/hearing).
//================================================================================
void CBot::SelectPostConditions()
{
    VPROF_BUDGET( "SelectPostConditions", VPROF_BUDGETGROUP_BOTS );

    SelectEnemyConditions();
    SelectAttackConditions();
}

//================================================================================
// Obtains new conditions related to health and damage
//================================================================================
void CBot::SelectHealthConditions()
{
    VPROF_BUDGET("SelectHealthConditions", VPROF_BUDGETGROUP_BOTS);

    ClearCondition( BCOND_LOW_HEALTH );
    ClearCondition( BCOND_LIGHT_DAMAGE );
    ClearCondition( BCOND_HEAVY_DAMAGE );
    ClearCondition( BCOND_REPEATED_DAMAGE );
    ClearCondition( BCOND_DEJECTED );

    if ( GetDecision()->IsLowHealth() ) {
        SetCondition( BCOND_LOW_HEALTH );
    }

#ifdef INSOURCE_DLL
    if ( GetHost()->IsDejected() ) {
        SetCondition( BCOND_DEJECTED );
    }

    if ( GetHost()->GetLastDamageTimer().IsGreaterThen( 3.0f ) ) {
        m_iRepeatedDamageTimes = 0;
        m_flDamageAccumulated = 0.0f;
    }

    if ( m_iRepeatedDamageTimes == 0 )
        return;

    if ( m_iRepeatedDamageTimes <= 2 ) {
        SetCondition( BCOND_LIGHT_DAMAGE );
    }
    else {
        SetCondition( BCOND_REPEATED_DAMAGE );
    }

    if ( m_flDamageAccumulated >= 30.0f ) {
        SetCondition( BCOND_HEAVY_DAMAGE );
    }
#endif
}

//================================================================================
// Gets new conditions related to current weapon
//================================================================================
void CBot::SelectWeaponConditions()
{
    VPROF_BUDGET( "SelectWeaponConditions", VPROF_BUDGETGROUP_BOTS );

    ClearCondition( BCOND_EMPTY_PRIMARY_AMMO );
    ClearCondition( BCOND_LOW_PRIMARY_AMMO );
    ClearCondition( BCOND_EMPTY_CLIP1_AMMO );
    ClearCondition( BCOND_LOW_CLIP1_AMMO );
    ClearCondition( BCOND_EMPTY_SECONDARY_AMMO );
    ClearCondition( BCOND_LOW_SECONDARY_AMMO );
    ClearCondition( BCOND_EMPTY_CLIP2_AMMO );
    ClearCondition( BCOND_LOW_CLIP2_AMMO );
    ClearCondition( BCOND_HELPLESS );

    // We change to the best weapon for this situation
    // TODO: A better place to put this.
    GetDecision()->SwitchToBestWeapon();

    CBaseWeapon *pWeapon = GetHost()->GetActiveBaseWeapon();

    if ( !pWeapon ) {
        SetCondition( BCOND_HELPLESS );
        return;
    }

    // Primary ammunition
    {
        int ammo = 0;
        int totalAmmo = GetHost()->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
        int totalRange = 30;

        if ( pWeapon->UsesClipsForAmmo1() ) {
            ammo = pWeapon->Clip1();
            int maxAmmo = pWeapon->GetMaxClip1();
            totalRange = (maxAmmo * 0.5);

            if ( ammo == 0 )
                SetCondition( BCOND_EMPTY_CLIP1_AMMO );
            else if ( ammo < totalRange )
                SetCondition( BCOND_LOW_CLIP1_AMMO );
        }

        if ( totalAmmo == 0 )
            SetCondition( BCOND_EMPTY_PRIMARY_AMMO );
        else if ( totalAmmo < totalRange )
            SetCondition( BCOND_LOW_PRIMARY_AMMO );
    }

    // Secondary ammunition
    {
        int ammo = 0;
        int totalAmmo = GetHost()->GetAmmoCount( pWeapon->GetSecondaryAmmoType() );
        int totalRange = 15;

        if ( pWeapon->UsesClipsForAmmo2() ) {
            ammo = pWeapon->Clip2();
            int maxAmmo = pWeapon->GetMaxClip2();
            totalRange = (maxAmmo * 0.5);

            if ( ammo == 0 )
                SetCondition( BCOND_EMPTY_CLIP2_AMMO );
            else if ( ammo < totalRange )
                SetCondition( BCOND_LOW_CLIP2_AMMO );
        }

        if ( totalAmmo == 0 )
            SetCondition( BCOND_EMPTY_SECONDARY_AMMO );
        else if ( totalAmmo < totalRange )
            SetCondition( BCOND_LOW_SECONDARY_AMMO );
    }

    // You have no ammunition of any kind, you are defenseless
    if ( HasCondition( BCOND_EMPTY_PRIMARY_AMMO ) &&
         HasCondition( BCOND_EMPTY_CLIP1_AMMO ) &&
         HasCondition( BCOND_EMPTY_SECONDARY_AMMO ) &&
         HasCondition( BCOND_EMPTY_CLIP2_AMMO ) )
        SetCondition( BCOND_HELPLESS );
}

//================================================================================
// Gets new conditions related to the current enemy
//================================================================================
void CBot::SelectEnemyConditions()
{
    VPROF_BUDGET( "SelectEnemyConditions", VPROF_BUDGETGROUP_BOTS );

    ClearCondition( BCOND_ENEMY_LOST );
    ClearCondition( BCOND_ENEMY_OCCLUDED );
    ClearCondition( BCOND_ENEMY_LAST_POSITION_OCCLUDED );
    ClearCondition( BCOND_ENEMY_DEAD );
    ClearCondition( BCOND_ENEMY_UNREACHABLE );
    ClearCondition( BCOND_ENEMY_TOO_NEAR );
    ClearCondition( BCOND_ENEMY_NEAR );
    ClearCondition( BCOND_ENEMY_FAR );
    ClearCondition( BCOND_ENEMY_TOO_FAR );
    ClearCondition( BCOND_NEW_ENEMY );
    ClearCondition( BCOND_SEE_ENEMY );

    if ( !GetEnemy() || !GetMemory() )
        return;

    CEntityMemory *memory = GetMemory()->GetEntityMemory();

    if ( !memory )
        return;

    if ( !IsCombating() ) {
        Alert();
    }

    const float distances[] = {
        100.0f,
        350.0f,
        800.0f,
        1300.0f
    };

    float enemyDistance = memory->GetDistance();

    if ( enemyDistance <= distances[0] )
        SetCondition( BCOND_ENEMY_TOO_NEAR );
    if ( enemyDistance <= distances[1] )
        SetCondition( BCOND_ENEMY_NEAR );
    if ( enemyDistance >= distances[2] )
        SetCondition( BCOND_ENEMY_FAR );
    if ( enemyDistance >= distances[3] )
        SetCondition( BCOND_ENEMY_TOO_FAR );

    /*if ( ) {
        SetCondition( BCOND_ENEMY_LAST_POSITION_OCCLUDED );
    }*/

    // No tenemos visi�n de ning�n hitbox del enemigo
    // @TODO: Que pasa con los enemigos que no tienen hitbox
    if ( !memory->IsVisible() ) {
        SetCondition( BCOND_ENEMY_OCCLUDED );

        if ( memory->GetElapsedTimeSinceVisible() > 3.0f ) {
            SetCondition( BCOND_ENEMY_LOST );
        }            
    }
    else {
        SetCondition( BCOND_SEE_ENEMY );
    }

    if ( !GetEnemy()->IsAlive() ) {
        if ( GetSkill()->IsEasy() ) {
            if ( GetEnemy()->m_lifeState == LIFE_DEAD ) {
                SetCondition( BCOND_ENEMY_DEAD );
            }
        }
        else {
            SetCondition( BCOND_ENEMY_DEAD );
        }
    }
}

//================================================================================
// Obtiene nuevas condiciones relacionadas al ataque
//================================================================================
void CBot::SelectAttackConditions()
{
    VPROF_BUDGET("SelectAttackConditions", VPROF_BUDGETGROUP_BOTS);

    // Limpiamos condiciones
    ClearCondition( BCOND_TOO_CLOSE_TO_ATTACK );
    ClearCondition( BCOND_TOO_FAR_TO_ATTACK );
    ClearCondition( BCOND_NOT_FACING_ATTACK );
    ClearCondition( BCOND_BLOCKED_BY_FRIEND );

    ClearCondition( BCOND_CAN_RANGE_ATTACK1 );
    ClearCondition( BCOND_CAN_RANGE_ATTACK2 );
    ClearCondition( BCOND_CAN_MELEE_ATTACK1 );
    ClearCondition( BCOND_CAN_MELEE_ATTACK2 );

    BCOND condition = GetDecision()->ShouldRangeAttack1();

    if ( condition != BCOND_NONE )
        SetCondition( condition );

    condition = GetDecision()->ShouldRangeAttack2();

    if ( condition != BCOND_NONE )
        SetCondition( condition );

    condition = GetDecision()->ShouldMeleeAttack1();

    if ( condition != BCOND_NONE )
        SetCondition( condition );

    condition = GetDecision()->ShouldMeleeAttack2();

    if ( condition != BCOND_NONE )
        SetCondition( condition );
}

//================================================================================
//================================================================================
CSquad *CBot::GetSquad() 
{
    return GetHost()->GetSquad();
}

//================================================================================
//================================================================================
void CBot::SetSquad( CSquad *pSquad ) 
{
    GetHost()->SetSquad( pSquad );
}

//================================================================================
//================================================================================
void CBot::SetSquad( const char *name ) 
{
    GetHost()->SetSquad( name );
}

//================================================================================
//================================================================================
bool CBot::IsSquadLeader()
{
    if ( !GetSquad() )
        return false;

    return (GetSquad()->GetLeader() == GetHost());
}

//================================================================================
//================================================================================
CPlayer * CBot::GetSquadLeader()
{
    if ( !GetSquad() )
        return NULL;

    return GetSquad()->GetLeader();
}

//================================================================================
//================================================================================
CBot * CBot::GetSquadLeaderAI()
{
    if ( !GetSquad() )
        return NULL;

    if ( !GetSquad()->GetLeader() )
        return NULL;

    return GetSquad()->GetLeader()->GetAI();
}

//================================================================================
// Un miembro de nuestro escuadron ha recibido da�o
//================================================================================
void CBot::OnMemberTakeDamage( CPlayer *pMember, const CTakeDamageInfo & info ) 
{
    // Estoy a la defensiva, pero mi escuadron necesita ayuda
   // if ( ShouldHelpFriend() )
    {
        // TODO
    }

    // Han atacado a un jugador normal, reportemos a sus amigos Bots
    if ( !pMember->IsBot() && info.GetAttacker() ) {
        OnMemberReportEnemy( pMember, info.GetAttacker() );
    }
}

//================================================================================
//================================================================================
void CBot::OnMemberDeath( CPlayer *pMember, const CTakeDamageInfo & info ) 
{
    if ( !GetMemory() || !GetVision() )
        return;

    CEntityMemory *memory = GetMemory()->GetEntityMemory( pMember );

    if ( !memory )
        return;

    // �Amigo! �Noo! :'(
    if ( !GetSkill()->IsHardest() && memory->IsVisible() ) {
        GetVision()->LookAt( "Squad Member Death", pMember->GetAbsOrigin(), PRIORITY_VERY_HIGH, RandomFloat( 0.3f, 1.5f ) );
    }
}

//================================================================================
//================================================================================
void CBot::OnMemberReportEnemy( CPlayer *pMember, CBaseEntity *pEnemy ) 
{
    if ( !GetMemory() )
        return;

    // Posici�n estimada del enemigo
    Vector vecEstimatedPosition = pEnemy->WorldSpaceCenter();
    const float errorDistance = 100.0f;

    // Cuando un amigo nos reporta la posici�n de un enemigo siempre debe haber un margen de error,
    // un humano no puede saber la posici�n exacta hasta verlo con sus propios ojos.
    vecEstimatedPosition.x += RandomFloat( -errorDistance, errorDistance );
    vecEstimatedPosition.y += RandomFloat( -errorDistance, errorDistance );

    // Actualizamos nuestra memoria
    GetMemory()->UpdateEntityMemory( pEnemy, vecEstimatedPosition, pMember );
}
