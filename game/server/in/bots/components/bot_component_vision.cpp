//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// Authors: 
// Michael S. Booth (linkedin.com/in/michaelbooth), 2003
// Iv�n Bravo Bravo (linkedin.com/in/ivanbravobravo), 2017

#include "cbase.h"
#include "bots\bot.h"

#include "in_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//================================================================================
// Macros
//================================================================================

// Time in seconds between each random "look at".
#define LOOK_RANDOM_INTERVAL ( IsAlerted() ) ? RandomInt( 3, 10 ) : RandomInt( 6, 20 )

// Time in seconds between each look at interesting places.
#define LOOK_INTERESTING_INTERVAL ( IsAlerted() ) ? RandomInt( 2, 5 )  : RandomInt( 6, 15 )

//================================================================================
//================================================================================
void CBotVision::Update()
{
    LookNavigation();

    LookAround();

    LookAtThreat();

    Process();
}


//================================================================================
// Process the aiming system
// Author: Michael S. Booth (linkedin.com/in/michaelbooth), 2003
//================================================================================
void CBotVision::Process()
{
    VPROF_BUDGET( "CBotVision::Process", VPROF_BUDGETGROUP_BOTS );

    if ( !HasAimGoal() )
        return;

    // We have finished aiming our target
    if ( IsVisionTimeExpired() ) {
        Reset();
        return;
    }

    int speed = GetAimSpeed();
    CBotCmd *cmd = GetBot()->GetUserCommand();

    QAngle viewAngles( cmd->viewangles );  
    Vector lookPosition( m_vecLookGoal - GetHost()->EyePosition() );
    QAngle lookAngle;
    VectorAngles( lookPosition, lookAngle );

    float lookYaw = lookAngle.y;
    float lookPitch = lookAngle.x;

    // Constants
    // TODO: Find better values?
    const float damping = 25.0f;
    const float maxAccel = 3000.0f;

    float deltaT = m_flTickInterval;
    float stiffness = GetStiffness();

    // If we are to this tolerance of being able to aim at our target we do it in an instant way.
    const float onTargetTolerance = 0.5f;

    // If we are to this tolerance of being able to aim at our target we declare that we are already seeing it.
    const float aimTolerance = 1.5f;

    //
    // Yaw
    //
    float angleDiffYaw = AngleNormalize( lookYaw - viewAngles.y );

    if ( angleDiffYaw < onTargetTolerance && angleDiffYaw > -onTargetTolerance || speed == AIM_SPEED_INSTANT ) {
        m_flLookYawVel = 0.0f;
        viewAngles.y = lookYaw;
    }
    else {
        // simple angular spring/damper
        float accel = stiffness * angleDiffYaw - damping * m_flLookYawVel;

        // limit rate
        if ( accel > maxAccel )
            accel = maxAccel;
        else if ( accel < -maxAccel )
            accel = -maxAccel;

        m_flLookYawVel += deltaT * accel;
        viewAngles.y += deltaT * m_flLookYawVel;
    }

    //
    // Pitch
    //
    float angleDiffPitch = lookPitch - viewAngles.x;
    angleDiffPitch = AngleNormalize( angleDiffPitch );

    if ( angleDiffPitch < onTargetTolerance && angleDiffPitch > -onTargetTolerance || speed == AIM_SPEED_INSTANT ) {
        m_flLookPitchVel = 0.0f;
        viewAngles.x = lookPitch;
    }
    else {
        // simple angular spring/damper
        // double the stiffness since pitch is only +/- 90 and yaw is +/- 180
        float accel = 2.0f * stiffness * angleDiffPitch - damping * m_flLookPitchVel;

        // limit rate
        if ( accel > maxAccel )
            accel = maxAccel;
        else if ( accel < -maxAccel )
            accel = -maxAccel;

        m_flLookPitchVel += deltaT * accel;
        viewAngles.x += deltaT * m_flLookPitchVel;
    }

    // We are in tolerance
    if ( (angleDiffYaw < aimTolerance && angleDiffYaw > -aimTolerance) && (angleDiffPitch < aimTolerance && angleDiffPitch > -aimTolerance) ) {
        m_bAimReady = true;

        // We start the timer
        if ( !m_VisionTimer.HasStarted() && m_flDuration > 0 ) {
            m_VisionTimer.Start( m_flDuration );
        }
    }
    else {
        m_bAimReady = false;
    }

    cmd->viewangles = viewAngles;
    Utils::DeNormalizeAngle( cmd->viewangles.x );
    Utils::DeNormalizeAngle( cmd->viewangles.y );
}

//================================================================================
// Sets [vecLookAt] as the ideal position to look at the specified entity
//================================================================================
void CBotVision::GetEntityBestAimPosition( CBaseEntity *pEntity, Vector &vecLookAt )
{
    vecLookAt.Invalidate();

    if ( !pEntity )
        return;

    CEntityMemory *memory = NULL;
    HitboxType favoriteHitbox = GetSkill()->GetFavoriteHitbox();

    if ( GetMemory() ) {
        memory = GetMemory()->GetEntityMemory( pEntity );
    }

    // We have information about this entity in our memory!
    if ( memory ) {
        if ( !memory->GetVisibleHitboxPosition( vecLookAt, favoriteHitbox ) ) {
            // We do not have a hitbox visible
            vecLookAt = memory->GetLastKnownPosition();
        }
        else {
            // We update the last position known as the last position of the hitbox
            memory->UpdatePosition( vecLookAt );
        }
    }
    // If it is a character, we try to aim to a hitbox
    else if ( pEntity->MyCombatCharacterPointer() ) {
        // The entity is our main threat, 
        // but it should be managed with memory, how did we get here?
        Assert( (GetBot()->GetEnemy() != pEntity) );

        Utils::GetHitboxPosition( pEntity, vecLookAt, favoriteHitbox );

        if ( !GetHost()->IsAbleToSee( vecLookAt, CBaseCombatCharacter::DISREGARD_FOV ) ) {
            vecLookAt = pEntity->WorldSpaceCenter();
        }
    }
    else {
        vecLookAt = pEntity->WorldSpaceCenter();
    }

    // No margin of error is required if we do not have visibility
    if ( memory && !memory->IsVisible() )
        return;

    // We added a margin of error when aiming.
    if ( GetSkill()->GetLevel() < SKILL_HARDEST ) {
        float errorRange = 0.0f;

        if ( GetSkill()->GetLevel() >= SKILL_HARD ) {
            errorRange = RandomFloat( 0.0f, 3.5f );
        }
        if ( GetSkill()->IsMedium() ) {
            errorRange = RandomFloat( 2.0f, 8.0f );
        }
        else {
            errorRange = RandomFloat( 5.0f, 10.0f );
        }

        vecLookAt.x += RandomFloat( -errorRange, errorRange );
        vecLookAt.y += RandomFloat( -errorRange, errorRange );
        vecLookAt.z += RandomFloat( -errorRange, errorRange );
    }
}

//================================================================================
// Returns ideal aiming speed
//================================================================================
int CBotVision::GetAimSpeed()
{
    int speed = GetSkill()->GetMinAimSpeed();

    if ( speed == AIM_SPEED_INSTANT ) {
        return AIM_SPEED_INSTANT;
    }

    // TODO: This should not be necessary.
    // (We need to predict where our target will be)
    if ( GetHost()->IsMoving() ) {
        ++speed;
    }

    // Adrenaline?
    if ( !GetSkill()->IsEasy() ) {
        if ( IsCombating() ) {
            ++speed;
        }

        if ( GetHost()->IsUnderAttack() ) {
            ++speed;
        }
    }

    speed = clamp( speed, GetSkill()->GetMinAimSpeed(), GetSkill()->GetMaxAimSpeed() );
    return speed;
}

//================================================================================
// TODO: Find better values?
//================================================================================
float CBotVision::GetStiffness()
{
    int speed = GetAimSpeed();

    switch ( speed ) {
        case AIM_SPEED_VERYLOW:
            return 90.0f;
            break;

        case AIM_SPEED_LOW:
            return 110.0f;
            break;

        case AIM_SPEED_NORMAL:
        default:
            return 150.0f;
            break;

        case AIM_SPEED_FAST:
            return 180.0f;
            break;

        case AIM_SPEED_VERYFAST:
            return 200.0f;
            break;

        case AIM_SPEED_INSTANT:
            return 999.0f;
            break;
    }
}

//================================================================================
// Look at the specified entity
//================================================================================
bool CBotVision::LookAt( const char *pDesc, CBaseEntity *pTarget, int priority, float duration )
{
    if ( !pTarget )
        return false;

    Vector vecLookAt;
    GetEntityBestAimPosition( pTarget, vecLookAt );

    return LookAt( pDesc, pTarget, vecLookAt, priority, duration );
}

//================================================================================
// Look at the specified location stating that it is an entity
//================================================================================
bool CBotVision::LookAt( const char *pDesc, CBaseEntity *pTarget, const Vector &vecLookAt, int priority, float duration )
{
    if ( !pTarget )
        return false;

    bool success = LookAt( pDesc, vecLookAt, priority, duration );

    if ( success ) {
        m_pLookingAt = pTarget;
    }

    return success;
}

//================================================================================
// Look at the specified location
//================================================================================
bool CBotVision::LookAt( const char *pDesc, const Vector &vecGoal, int priority, float duration )
{
    if ( !vecGoal.IsValid() )
        return false;

    if ( GetPriority() > priority )
        return false;

    m_vecLookGoal = vecGoal;
    m_pLookingAt = NULL;
    m_pDescription = pDesc;
    m_bAimReady = false;
    m_flDuration = duration;
    m_VisionTimer.Invalidate();

    SetPriority( priority );

    // We avoid spam...
    /*if ( !FStrEq( "Looking Forward", pDesc ) && !FStrEq( "Threat", pDesc ) ) {
        GetBot()->DebugAddMessage( "LookAt(%s, %.2f %.2f, %i, %.2f)", m_pDescription, m_vecLookGoal.x, m_vecLookGoal.y, priority, duration );
    }*/
}

//================================================================================
//================================================================================
void CBotVision::LookAtThreat()
{
    if ( !GetMemory() )
        return;

    CEntityMemory *memory = GetMemory()->GetPrimaryThreat();

    if ( memory == NULL )
        return;

    if ( memory->IsLost() )
        return;

    if ( !GetDecision()->ShouldLookThreat() )
        return;

    // It's very close! We look directly at its center.
    // This solves several problems when attempting to aim to a hitbox when it is too close.
    if ( memory->GetDistance() <= 80.0f ) {
        LookAt( "Threat Center", memory->GetEntity()->WorldSpaceCenter(), PRIORITY_HIGH, 0.2f );
        return;
    }    

    LookAt( "Threat", memory->GetEntity(), PRIORITY_HIGH, 0.2f );
}

//================================================================================
// Look to the next position on the route
//================================================================================
void CBotVision::LookNavigation()
{
    if ( !GetLocomotion() )
        return;

    if ( !GetLocomotion()->HasDestination() )
        return;

    Vector lookAt( GetLocomotion()->GetNextSpot() );
    lookAt.z = GetHost()->EyePosition().z;

    int priority = PRIORITY_LOW;

    if ( GetFollow() && GetFollow()->IsFollowingActive() ) {
        priority = PRIORITY_NORMAL;
    }

    LookAt( "Looking Forward", lookAt, priority, 0.5f );
}

//================================================================================
// It allows us to look at an interesting or random place.
//================================================================================
void CBotVision::LookAround()
{
    if ( GetMemory() ) {
        int blocked = GetDataMemoryInt( "BlockLookAround" );

        if ( blocked == 1 )
            return;
    }

    // We heard a sound that represents danger
    if ( HasCondition( BCOND_HEAR_COMBAT ) || HasCondition( BCOND_HEAR_DANGER ) || HasCondition( BCOND_HEAR_ENEMY ) ) {
        if ( GetDecision()->ShouldLookDangerSpot() ) {
            LookDanger();
            return;
        }
    }

    if ( GetPriority() > PRIORITY_NORMAL )
        return;

    // An interesting place:
    // Places where enemies can be covered or revealed.
    if ( GetDecision()->ShouldLookInterestingSpot() ) {
        LookInterestingSpot();
        return;
    }

    // Random place
    if ( GetDecision()->ShouldLookRandomSpot() ) {
        LookRandomSpot();
        return;
    }

    // We are in a squadron, look at a friend :)
    if ( GetDecision()->ShouldLookSquadMember() ) {
        LookSquadMember();
        return;
    }
}


//================================================================================
// Find an interesting place and look at that place
//================================================================================
void CBotVision::LookInterestingSpot()
{
    CBotDecision *pDecision = dynamic_cast<CBotDecision *>(GetBot()->GetDecision());
    Assert( pDecision );

    pDecision->m_IntestingAimTimer.Start( LOOK_INTERESTING_INTERVAL );

    CSpotCriteria criteria;
    criteria.SetMaxRange( 1000.0f );
    criteria.OnlyVisible( !GetDecision()->CanLookNoVisibleSpots() );
    criteria.UseRandom( true );
    criteria.SetTacticalMode( GetBot()->GetTacticalMode() );

    Vector vecSpot;

    if ( !Utils::FindIntestingPosition( &vecSpot, GetHost(), criteria ) )
        return;

    LookAt( "Intesting Spot", vecSpot, PRIORITY_NORMAL, 1.0f );
}

//================================================================================
// Look at random spot
//================================================================================
void CBotVision::LookRandomSpot()
{
    CBotDecision *pDecision = dynamic_cast<CBotDecision *>( GetBot()->GetDecision() );
    Assert( pDecision );

    pDecision->m_RandomAimTimer.Start( LOOK_RANDOM_INTERVAL );

    QAngle viewAngles = GetBot()->GetUserCommand()->viewangles;
    viewAngles.x += RandomInt( -10, 10 );
    viewAngles.y += RandomInt( -40, 40 );

    Vector vecForward;
    AngleVectors( viewAngles, &vecForward );

    Vector vecPosition = GetHost()->EyePosition();
    LookAt( "Random Spot", vecPosition + 30 * vecForward, PRIORITY_VERY_LOW );
}

//================================================================================
// We look at a member of our squad
//================================================================================
void CBotVision::LookSquadMember()
{
    CPlayer *pMember = GetHost()->GetSquad()->GetRandomMember();

    if ( !pMember || pMember == GetHost() ) {
        return;
    }

    LookAt( "Squad Member", pMember, PRIORITY_VERY_LOW, RandomFloat(1.0f, 3.5f) );
}

//================================================================================
// Look to a place where we heard danger/combat
//================================================================================
void CBotVision::LookDanger()
{
    CSound *pSound = GetHost()->GetBestSound( SOUND_COMBAT | SOUND_PLAYER | SOUND_DANGER );

    if ( !pSound )
        return;

    Vector vecLookAt = pSound->GetSoundReactOrigin();
    vecLookAt.z = GetHost()->EyePosition().z;

    LookAt( "Danger Sound", vecLookAt, PRIORITY_HIGH, RandomFloat( 1.0f, 3.5f ) );
}