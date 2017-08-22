//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// Authors: 
// Iv�n Bravo Bravo (linkedin.com/in/ivanbravobravo), 2017

#ifndef IBOT_COMPONENT_H
#define IBOT_COMPONENT_H

#ifdef _WIN32
#pragma once
#endif

#include "ibot.h"

class IBotVision;
class IBotAttack;
class IBotMemory;
class IBotLocomotion;
class IBotFollow;
class IBotDecision;

//================================================================================
// Macros
//================================================================================

#define FOR_EACH_COMPONENT FOR_EACH_MAP( m_nComponents, it )
#define DECLARE_COMPONENT( id ) virtual int GetID() const { return id; }

//================================================================================
// Artificial Intelligence Component.
// Base for creating components and schedules.
//================================================================================
abstract_class IBotComponent : public CPlayerInfo
{
public:
    IBotComponent( IBot *bot )
    {
        this->Reset();

        m_nBot = bot;
        m_pParent = m_nBot->m_pParent;
    }

    virtual bool IsSchedule() const {
        return false;
    }

    virtual IBot *GetBot() const {
        return m_nBot;
    }

    virtual CPlayer *GetHost() const {
        return ToInPlayer( m_pParent );
    }

    virtual void Reset() {
        m_flTickInterval = gpGlobals->interval_per_tick;
    }

    virtual CBotSkill *GetSkill() const {
        return m_nBot->GetSkill();
    }

    virtual void SetCondition( BCOND condition ) {
        m_nBot->SetCondition( condition );
    }

    virtual void ClearCondition( BCOND condition ) {
        m_nBot->ClearCondition( condition );
    }

    virtual bool HasCondition( BCOND condition ) const {
        return m_nBot->HasCondition( condition );
    }

    virtual void InjectButton( int btn ) {
        m_nBot->InjectButton( btn );
    }

    virtual bool IsIdle() const {
        return m_nBot->IsIdle();
    }

    virtual bool IsAlerted() const {
        return m_nBot->IsAlerted();
    }

    virtual bool IsCombating() const {
        return m_nBot->IsCombating();
    }

    virtual bool IsPanicked() const {
        return m_nBot->IsPanicked();
    }

    virtual IBotVision *GetVision() const {
        return m_nBot->GetVision();
    }

    virtual IBotAttack *GetAttack() const {
        return m_nBot->GetAttack();
    }

    virtual IBotMemory *GetMemory() const {
        return m_nBot->GetMemory();
    }

    virtual IBotLocomotion *GetLocomotion() const {
        return m_nBot->GetLocomotion();
    }

    virtual IBotFollow *GetFollow() const {
        return m_nBot->GetFollow();
    }

    virtual IBotDecision *GetDecision() const {
        return m_nBot->GetDecision();
    }

public:
    virtual int GetID() const = 0;
    virtual void Update() = 0;

public:
    float m_flTickInterval;

protected:
    IBot *m_nBot;
};

#endif // IBOT_COMPONENT_H