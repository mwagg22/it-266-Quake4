
#include "../../idlib/precompiled.h"
#pragma hdrstop
#include "../Game_local.h"

class rvMonsterGrunt : public idAI {
public:

	CLASS_PROTOTYPE( rvMonsterGrunt );

	rvMonsterGrunt ( void );
	
	void				Spawn					( void );
	void				Save					( idSaveGame *savefile ) const;
	void				Restore					( idRestoreGame *savefile );
	void				Damage(idEntity *inflictor, idEntity *attacker, const idVec3 &dir, const char *damageDefName, const float damageScale, const int location);
	virtual void		AdjustHealthByDamage	( int damage );
	int					AffinityType;
protected:

	rvAIAction			actionMeleeMoveAttack;
	rvAIAction			actionChaingunAttack;

	virtual bool		CheckActions		( void );

	virtual void		OnTacticalChange	( aiTactical_t oldTactical );
	virtual void		OnDeath				( void );

private:

	int					standingMeleeNoAttackTime;
	int					rageThreshold;

	void				RageStart			( void );
	void				RageStop			( void );
	
	// Torso States
	stateResult_t		State_Torso_Enrage		( const stateParms_t& parms );
	stateResult_t		State_Torso_Pain		( const stateParms_t& parms );
	stateResult_t		State_Torso_LeapAttack	( const stateParms_t& parms );

	CLASS_STATES_PROTOTYPE ( rvMonsterGrunt );
};

CLASS_DECLARATION( idAI, rvMonsterGrunt )
END_CLASS

/*
================
rvMonsterGrunt::rvMonsterGrunt
================
*/
rvMonsterGrunt::rvMonsterGrunt ( void ) {
	standingMeleeNoAttackTime = 0;
}

/*
================
rvMonsterGrunt::Spawn
================
*/
void rvMonsterGrunt::Spawn ( void ) {
	rageThreshold = spawnArgs.GetInt ( "health_rageThreshold" );

	// Custom actions
	actionMeleeMoveAttack.Init	( spawnArgs, "action_meleeMoveAttack",	NULL,				AIACTIONF_ATTACK );
	actionChaingunAttack.Init	( spawnArgs, "action_chaingunAttack",	NULL,				AIACTIONF_ATTACK );
	actionLeapAttack.Init		( spawnArgs, "action_leapAttack",		"Torso_LeapAttack",	AIACTIONF_ATTACK );
	AffinityType = 1;	//random affinity number from 0-2; 0:fire,1:lighning,2:water/dark?
	gameLocal.Printf("alive", AffinityType);
	// Enraged to start?
	if ( spawnArgs.GetBool ( "preinject" ) ) {
		RageStart ( );
	}	
}

/*
================
rvMonsterGrunt::Save
================
*/
void rvMonsterGrunt::Save ( idSaveGame *savefile ) const {
	actionMeleeMoveAttack.Save( savefile );
	actionChaingunAttack.Save( savefile );

	savefile->WriteInt( rageThreshold );
	savefile->WriteInt( standingMeleeNoAttackTime );
}

/*
================
rvMonsterGrunt::Restore
================
*/
void rvMonsterGrunt::Restore ( idRestoreGame *savefile ) {
	actionMeleeMoveAttack.Restore( savefile );
	actionChaingunAttack.Restore( savefile );

	savefile->ReadInt( rageThreshold );
	savefile->ReadInt( standingMeleeNoAttackTime );
}

/*
================
rvMonsterGrunt::RageStart
================
*/
void rvMonsterGrunt::RageStart ( void ) {
	SetShaderParm ( 6, 1 );

	// Disable non-rage actions
	actionEvadeLeft.fl.disabled = true;
	actionEvadeRight.fl.disabled = true;
	
	// Speed up animations
	animator.SetPlaybackRate ( 1.25f );

	// Disable pain
	pain.threshold = 0;

	// Start over with health when enraged
	health = spawnArgs.GetInt ( "health" );
	
	// No more going to rage
	rageThreshold = 0;
}

/*
================
rvMonsterGrunt::RageStop
================
*/
void rvMonsterGrunt::RageStop ( void ) {
	SetShaderParm ( 6, 0 );
}

/*
================
rvMonsterGrunt::CheckActions
================
*/
bool rvMonsterGrunt::CheckActions ( void ) {
	// If our health is below the rage threshold then enrage
	if ( health < rageThreshold ) { 
		PerformAction ( "Torso_Enrage", 4, true );
		return true;
	}

	// Moving melee attack?
	if ( PerformAction ( &actionMeleeMoveAttack, (checkAction_t)&idAI::CheckAction_MeleeAttack, NULL ) ) {
		return true;
	}
	
	// Default actions
	if ( CheckPainActions ( ) ) {
		return true;
	}

	if ( PerformAction ( &actionEvadeLeft,   (checkAction_t)&idAI::CheckAction_EvadeLeft, &actionTimerEvade )			 ||
			PerformAction ( &actionEvadeRight,  (checkAction_t)&idAI::CheckAction_EvadeRight, &actionTimerEvade )			 ||
			PerformAction ( &actionJumpBack,	 (checkAction_t)&idAI::CheckAction_JumpBack, &actionTimerEvade )			 ||
			PerformAction ( &actionLeapAttack,  (checkAction_t)&idAI::CheckAction_LeapAttack )	) {
		return true;
	} else if ( PerformAction ( &actionMeleeAttack, (checkAction_t)&idAI::CheckAction_MeleeAttack ) ) {
		standingMeleeNoAttackTime = 0;
		return true;
	} else {
		if ( actionMeleeAttack.status != rvAIAction::STATUS_FAIL_TIMER
			&& actionMeleeAttack.status != rvAIAction::STATUS_FAIL_EXTERNALTIMER
			&& actionMeleeAttack.status != rvAIAction::STATUS_FAIL_CHANCE )
		{//melee attack fail for any reason other than timer?
			if ( combat.tacticalCurrent == AITACTICAL_MELEE && !move.fl.moving )
			{//special case: we're in tactical melee and we're close enough to think we've reached the enemy, but he's just out of melee range!
				if ( !standingMeleeNoAttackTime )
				{
					standingMeleeNoAttackTime = gameLocal.GetTime();
				}
				else if ( standingMeleeNoAttackTime + 2500 < gameLocal.GetTime() )
				{//we've been standing still and not attacking for at least 2.5 seconds, fall back to ranged attack
					//allow ranged attack
					actionRangedAttack.fl.disabled = false;
				}
			}
		}
		if ( PerformAction ( &actionRangedAttack,(checkAction_t)&idAI::CheckAction_RangedAttack, &actionTimerRangedAttack ) ) {
			return true;
		}
	}
	return false;
}

/*
================
rvMonsterGrunt::OnDeath
================
*/
void rvMonsterGrunt::OnDeath ( void ) {
	RageStop ( );
	return idAI::OnDeath ( );
}

/*
================
rvMonsterGrunt::OnTacticalChange

Enable/Disable the ranged attack based on whether the grunt needs it
================
*/
void rvMonsterGrunt::OnTacticalChange ( aiTactical_t oldTactical ) {
	switch ( combat.tacticalCurrent ) {
		case AITACTICAL_MELEE:
			actionRangedAttack.fl.disabled = true;
			break;

		default:
			actionRangedAttack.fl.disabled = false;
			break;
	}
}

/*
=====================
rvMonsterGrunt::AdjustHealthByDamage
=====================
*/
void rvMonsterGrunt::AdjustHealthByDamage ( int damage ) {
	// Take less damage during enrage process 
	if ( rageThreshold && health < rageThreshold ) { 
		health -= (damage * 0.25f);
		return;
	}
	return idAI::AdjustHealthByDamage ( damage );
}
/*
============
Damage

this		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
example: this=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback in global space
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted

inflictor, attacker, dir, and point can be NULL for environmental effects

============
*/
void rvMonsterGrunt::Damage(idEntity *inflictor, idEntity *attacker, const idVec3 &dir,
	const char *damageDefName, const float damageScale, const int location) {
	if (forwardDamageEnt.IsValid()) {
		forwardDamageEnt->Damage(inflictor, attacker, dir, damageDefName, damageScale, location);
		return;
	}

	if (!fl.takedamage) {
		return;
	}

	if (!inflictor) {
		inflictor = gameLocal.world;
	}

	if (!attacker) {
		attacker = gameLocal.world;
	}

	const idDict *damageDef = gameLocal.FindEntityDefDict(damageDefName, false);
	if (!damageDef) {
		gameLocal.Error("Unknown damageDef '%s'\n", damageDefName);
	}

	int	damage = damageDef->GetInt("damage");

	// inform the attacker that they hit someone
	attacker->DamageFeedback(this, inflictor, damage);
	if (damage) {
		// do the damage
		//jshepard: this is kin   Child *pChild =  (Child *) &parent; da important, no?
		if (attacker->IsType(idPlayer::GetClassType())){
			idPlayer *attackerp = dynamic_cast<idPlayer *>(attacker);
			switch (attackerp->GetElement()){
			case 0:{
					   if (AffinityType == 0){
						   gameLocal.Printf("Nope", AffinityType);
						   if (attackerp->pfl.melee_attacking){
							   health -= (damage*1.25f);
						   }
						   else
						   health += (damage * 0.25f);
					break;
					   }
					   else if (AffinityType == 1){
						   gameLocal.Printf("ouch", AffinityType);
						   health -= (damage*1.5f);
						   break;
					   }
					   else{
						   gameLocal.Printf("aight", AffinityType);
						   health -= (damage*0.25f);
						   break;
					   }
					   
			}
			case 1:{
					   if (AffinityType == 0){
						   gameLocal.Printf("aight", AffinityType);
						   health -= (damage);
						   break;
					   }
					   else if (AffinityType == 1){
						   gameLocal.Printf("Nope", AffinityType);
						   if (attackerp->pfl.melee_attacking){
							   health -= (damage*1.25f);
						   }
						   else
						   health += (damage*.25f);
						   break;
					   }
					   else{
						   gameLocal.Printf("ouch", AffinityType);
						   health -= (damage*1.5f);
						   break;
					   }
			}
			case 2:{
					   if (AffinityType == 0){
						   gameLocal.Printf("ouch", AffinityType);
						   health -= (damage * 1.5f);
						   break;
					   }
					   else if (AffinityType == 1){
						   gameLocal.Printf("aight", AffinityType);
						   health -= damage;
						   break;
					   }
					   else{
						   gameLocal.Printf("Nope", AffinityType);
						   if (attackerp->pfl.melee_attacking){
							   health -= (damage*1.25f);
						   }
						   else
						   health += (damage*0.25f);
						   break;
					   }
			}
			}
		}
		else
		{
			health -= damage;
		}

		if (health <= 0) {
			if (health < -999) {
				health = -999;
			}

			Killed(inflictor, attacker, damage, dir, location);
		}
		else {
			Pain(inflictor, attacker, damage, dir, location);
		}
	}
}

/*


/*
===============================================================================

	States 

===============================================================================
*/

CLASS_STATES_DECLARATION ( rvMonsterGrunt )
	STATE ( "Torso_Enrage",		rvMonsterGrunt::State_Torso_Enrage )
	STATE ( "Torso_Pain",		rvMonsterGrunt::State_Torso_Pain )
	STATE ( "Torso_LeapAttack",	rvMonsterGrunt::State_Torso_LeapAttack )
END_CLASS_STATES

/*
================
rvMonsterGrunt::State_Torso_Pain
================
*/
stateResult_t rvMonsterGrunt::State_Torso_Pain ( const stateParms_t& parms ) {
	// Stop streaming pain if its time to get angry
	if ( pain.loopEndTime && health < rageThreshold ) {
		pain.loopEndTime = 0;
	}
	return idAI::State_Torso_Pain ( parms );
}

/*
================
rvMonsterGrunt::State_Torso_Enrage
================
*/
stateResult_t rvMonsterGrunt::State_Torso_Enrage ( const stateParms_t& parms ) {
	enum {
		STAGE_ANIM,
		STAGE_ANIM_WAIT,
	};
	switch ( parms.stage ) {
		case STAGE_ANIM:
			DisableAnimState ( ANIMCHANNEL_LEGS );
			PlayAnim ( ANIMCHANNEL_TORSO, "anger", parms.blendFrames );
			return SRESULT_STAGE ( STAGE_ANIM_WAIT );
		
		case STAGE_ANIM_WAIT:
			if ( AnimDone ( ANIMCHANNEL_TORSO, 4 ) ) {
				RageStart ( );
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}


/*
================
rvMonsterGrunt::State_Torso_LeapAttack
================
*/
stateResult_t rvMonsterGrunt::State_Torso_LeapAttack ( const stateParms_t& parms ) {
	enum {
		STAGE_ANIM,
		STAGE_ANIM_WAIT,
	};
	switch ( parms.stage ) {
		case STAGE_ANIM:
			DisableAnimState ( ANIMCHANNEL_LEGS );
			lastAttackTime = 0;
			// Play the action animation
			PlayAnim ( ANIMCHANNEL_TORSO, animator.GetAnim ( actionAnimNum )->FullName ( ), parms.blendFrames );
			return SRESULT_STAGE ( STAGE_ANIM_WAIT );
		
		case STAGE_ANIM_WAIT:
			if ( AnimDone ( ANIMCHANNEL_TORSO, parms.blendFrames ) ) {
				// If we missed our leap attack get angry
				if ( !lastAttackTime && rageThreshold ) {
					PostAnimState ( ANIMCHANNEL_TORSO, "Torso_Enrage", parms.blendFrames );
				}
				return SRESULT_DONE;
			}
			return SRESULT_WAIT;
	}
	return SRESULT_ERROR;
}
