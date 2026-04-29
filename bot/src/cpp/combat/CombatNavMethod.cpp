//
// $Id: CombatNavMethod.cpp,v 1.51 2007/09/04 17:13:32 masked_carrot Exp $

// Copyright (c) 2003, WhichBot Project
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the WhichBot Project nor the names of its
//       contributors may be used to endorse or promote products derived from
//       this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "CombatNavMethod.h"
#include "navigation/WaypointNavMethod.h"
#include <algorithm>
#include "extern/halflifesdk/util.h"
#include "BotManager.h"
#include "worldstate/HiveManager.h"
#include "strategy/FleeStrategy.h"
#include "strategy/HiveMind.h"
#include "config/EntityInfo.h"
#include "config/TranslationManager.h"
#include "worldstate/AreaManager.h"
#include "worldstate/WorldStateUtil.h"
#include "framework/Log.h"

const float MIN_MINE_RANGE = 256.0;

// Distance we have to be within in order to hit something.
// We set it a little low because the movement engine measures the x/y distance
const float NEAR_TARGET_DISTANCE = 30.0 ;

// If we're in active cloak (ie near a sens chamber) we don't attack over this range
const float MAX_CLOAK_ATTACK_RANGE = 80.0;

// If a new high priority target is in this range, we'll attack it
const float MELEE_RANGE = 100.0;

const float AIM_TIME = 0.1;



CombatNavMethod::CombatNavMethod(Bot& bot) :
    _bot(bot),
	_lastJumpTime(0.0),
	_lastStrafeTime(0.0),
	_lastTargetSeenTime(gpGlobals->time),
	_strafeSpeed(0.0),
	_strafeTime(0.0),
    _aimStartTime(0.0),
	_wptNav(bot, false)
{
}

class TargetSorter
{
public:
    TargetSorter(Vector& botOrigin, tEvolution botEvolution) :
      _botOrigin(botOrigin),
      _botEvolution(botEvolution)
    {
    }

    bool operator() (const Target& cLeft, const Target& cRight)
    {
        Target& left = (Target&)cLeft;
        Target& right = (Target&)cRight;
		// right now we just grab the highest attack target
        if (!left.isValid()) {
            return false;

        } else {
            if (!right.isValid()) {
                return true;
            }

            if (_botEvolution == kGorge) {
                // We pick injured players above other targets as a gorge
                if (left.getEntity().getEdict()->v.team == ALIEN_TEAM) {
                    return true;
                }
                if (right.getEntity().getEdict()->v.team == ALIEN_TEAM) {                   
                    return false;
                }
            }

            if (left.getInfo()->getAttackValue() != right.getInfo()->getAttackValue()) {
                return left.getInfo()->getAttackValue() > right.getInfo()->getAttackValue();

            } else {
                float leftRange = (_botOrigin - left.getOrigin()).Length();
                float rightRange = (_botOrigin - right.getOrigin()).Length();
                return leftRange < rightRange;
            }
        }
    }

    Vector _botOrigin;
    tEvolution _botEvolution;
};


const float TARGET_TIMEOUT = 5.0;


bool CombatNavMethod::targetExists()
{
    Target* pTarget = _bot.getTarget();
    return (pTarget != NULL && pTarget->isValid());
}


NavigationMethod* CombatNavMethod::navigate()
{
    if (targetExists()) {
		checkForTargetDeath();
    }

    // sanity check (defensive coding against a new round start)
    if (gpGlobals->time < _aimStartTime) {
        _aimStartTime = 0.0;
    }

    if ((_bot.getSensor() != NULL) && _bot.getSensor()->targetSeen()) {
		TargetVector& targets = _bot.getSensor()->getTargets();
		
		sort(targets.begin(), targets.end(), TargetSorter(_bot.getEdict()->v.origin, _bot.getEvolution()));
		
		// skip any targets which died since we did a sensor scan
		for (TargetVector::iterator ii = targets.begin(); ii != targets.end(); ii++) {
			if (assessTarget(*ii)) {
				break;
			}
		}
    }
	
    if ((gpGlobals->time - _lastTargetSeenTime) > 5.0) {
        WB_LOG_INFO("Timing out target");
        setTarget(NULL);
    }
	
    if (targetExists()) {
		if (canAttackNow()) {
			attack();

		} else {
			if (FleeStrategy::botIsScared(_bot) && !_bot.getSensor()->threatSeen()){
				return new WaypointNavMethod(_bot,false);
			}
			moveToAttackPosition();
		}
		return NULL;
		
    } else {
		return new WaypointNavMethod(_bot, false);
	}
}

void CombatNavMethod::attack()
{
    if (targetExists()) {
        Vector targetOrigin = _bot.getTarget()->getOrigin();
        WorldStateUtil::checkVector(targetOrigin);

        Vector targetOffset = calculateInterceptSolution(
            targetOrigin, 
            _bot.getEdict()->v.origin,
            _bot.getTarget()->getEntity().getEdict()->v.velocity,
            _bot.getEdict()->v.velocity.Length());

        Vector interceptOrigin = _bot.getEdict()->v.origin + targetOffset;
        WorldStateUtil::checkVector(interceptOrigin);
        
        // Hack the target origin slightly higher, so we don't aim right at the bottom and miss it.
		switch (_bot.getEvolution())
		{
		case kOnos:
			interceptOrigin.z += 3.5f;
			break;
		case kFade:
			interceptOrigin.z += 7.5f;
			break;
		default:
			interceptOrigin.z += 10.0f;
			break;
		}
		/*
		if (_bot.getEvolution() == kOnos)
			interceptOrigin.z += 3.0f;
		else
			interceptOrigin.z += 10.0f;
		*/

        float nearTargetDistance = NEAR_TARGET_DISTANCE;
        if (_bot.getEvolution() == kOnos) {
            // An Onos can't get that close to the target 'cos their snout is in the way
            nearTargetDistance *= 3.0;

            if (targetOffset.Length() < nearTargetDistance && _bot.getEdict()->v.origin.z - interceptOrigin.z > 0) {
                _bot.getEdict()->v.button |= IN_DUCK;
            }
        }
        // Fades have long swipe range so they don't need to get so close
        if (_bot.getEvolution() == kFade) {
            nearTargetDistance *= 1.6;
        }

        WorldStateUtil::checkVector(interceptOrigin);
		//WaypointDebugger::drawDebugBeam(_bot.getEdict()->v.origin, interceptOrigin, 255, 0, 0);
        _bot.getMovement()->setTarget(interceptOrigin, nearTargetDistance);
        Vector relativeVector(_bot.getTarget()->getOrigin() - _bot.getEdict()->v.origin);
        float range = relativeVector.Length();
        
        //WaypointDebugger::drawDebugBeam(_bot.getEdict()->v.origin, interceptOrigin, 255, 0, 0);
        if (!_bot.isCloaked() || range < MAX_CLOAK_ATTACK_RANGE) {
            if (_bot.selectWeapon(_bot.getTarget(), range, _bot.getMovement()->shouldUseMeleeAttack())) {
                if (_aimStartTime == 0.0) {
                    _aimStartTime = gpGlobals->time;
                }
            }

        } else {
            WB_LOG_INFO("Not attacking, cloaked");
        }

        if (range < nearTargetDistance && (_bot.getEvolution() == kFade || _bot.getEvolution() == kOnos))
        {
            // if we're an onos or fade, we're close up and the target is crouching, let's crouch too
            if (_bot.getTarget()->getOrigin().z <= _bot.getEdict()->v.origin.z /*&& 
                (_bot.getTarget()->getEntity().getEdict()->v.button & IN_DUCK) != 0*/)
            {
                _bot.getEdict()->v.button |= IN_DUCK;
            }
        }

        if (_aimStartTime != 0.0) {
            if (_bot.getTarget() != NULL) {
                float offsetAngle = _bot.getMovement()->getRelativeHitAngleDeg(relativeVector);

                if (gpGlobals->time > _aimStartTime + AIM_TIME || offsetAngle < 20) {
                    if (_bot.selectWeapon(_bot.getTarget(), range, _bot.getMovement()->shouldUseMeleeAttack())) {
						_bot.fireWeapon();
                    }
                    _aimStartTime = 0.0;
                }
            }


        } else if (_aimStartTime == 0.0) {
            evade(range);
        }

        // Experimental.  If we're a gorge, don't get too close.
        if (_bot.getEvolution() == kGorge && 
            BotSensor::isElectrified(_bot.getTarget()->getEntity().getEdict()) &&
            range < ELECTRIFIED_RANGE * 1.4)
        {
            _bot.getMovement()->stop(kGorge);
        }
        
        _bot.getMovement()->move(_bot.getEvolution());
    }
}


bool CombatNavMethod::canAttackNow()
{
    if (!targetExists()) {
        return false;
    }
    
    Vector targetOrigin(_bot.getTarget()->getOrigin());
	float range = (_bot.getEdict()->v.origin - targetOrigin).Length();

	// if we're scared and we're not killing our player target right now
	if (FleeStrategy::botIsScared(_bot) && _bot.getTarget()->isValid()) {
		//if ((_bot.getTarget()->getInfo()->isBuilding() || (range > 100.0))) {
		if ((_bot.getTarget()->getInfo()->isBuilding() && 
			(range > 100.0 || (_bot.getTarget()->getEntity().getEdict()->v.iuser4 & MASK_UPGRADE_11)))) { // far away or electrified
			return false;
		}
	}

	// For now, we'll assume that skulks, lerks and gorges can always attack if they're not scared
	if ((_bot.getEvolution() == kSkulk) || (_bot.getEvolution() == kGorge) || (_bot.getEvolution() == kLerk)) {
		return true;
	}

	//tmc - onos can stomp with 2 and charge with 3 hives.
	if (_bot.getEvolution() == kOnos){
		bool twoHives = gpBotManager->inCombatMode() ? _bot.hasTrait(Config::getInstance().getImpulse(IMPULSE_HIVE2)) : (HiveManager::getActiveHiveCount() > 1);
	//TODO: find out the effective range of stomp.
		if (twoHives) return true;
	}
	
	//tmc - fades can use acid rocket with 3 hives.
	if (_bot.getEvolution() == kFade )
	{
		bool threeHives = gpBotManager->inCombatMode() ? _bot.hasTrait(Config::getInstance().getImpulse(IMPULSE_HIVE3)):(HiveManager::getActiveHiveCount()==3);

		if (threeHives)	{
		
			return true;
		}
	}
	// Fades and Onos, on the other hand, have to get up close and personal
	if (range > 300.0) {
		return false;
	}

	return gpBotManager->getWaypointManager().isPathWalkable(_bot.getEdict()->v.origin, targetOrigin);
}


void CombatNavMethod::moveToAttackPosition()
{
	_wptNav.navigate();
}


void CombatNavMethod::checkForTargetDeath()
{
    // check if our target died
    if (!_bot.getTarget()->isValid() || (_bot.getTarget()->getEntity().getEdict()->v.health <= 0)) {
		_bot.getEdict()->v.impulse = Config::getInstance().getImpulse(IMPULSE_CHUCKLE);
		if (_bot.getTarget()->getInfo()->isBuilding()) {
			WB_LOG_INFO("Destroyed building {}, removing influencing entity", _targetName);
			HiveMind::entityDestroyed(_bot.getTarget()->getEntity().getEdict());
			Bot* pBot = gpBotManager->getBot(_bot.getEdict());
			if (pBot != NULL) {
				if (strstr(_targetName.c_str(), "mine") == NULL && gpBotManager->isBotChatEnabled()) {
					pBot->sayToTeam(
						TranslationManager::getTranslation("hivemind_say_prefix") + " " +
						TranslationManager::getTranslation("just_destroyed") + " " +
						TranslationManager::getTranslation(_targetName));
				}
			}
		} else {
			WB_LOG_INFO("Ate a {}", _bot.getTarget()->getInfo()->getClassname());
		}
		
		setTarget(NULL);
    }
}


bool CombatNavMethod::assessTarget(Target& potentialTarget)
{
    if (!potentialTarget.getEntity().isNull()) {
		if (potentialTarget.getEntity().getEdict()->v.health > 0) {
			tEvolution evolution = _bot.getEvolution();
			
			bool targetIsMine = false;
			if (potentialTarget.getInfo() != NULL) {
				targetIsMine = ((potentialTarget.getInfo()->getType() == kTripMine) || 
					(potentialTarget.getInfo()->getType() == kLandMine));
			}
			Vector vecToTarget(potentialTarget.getEntity().getEdict()->v.origin - _bot.getEdict()->v.origin);
			
            if (evolution == kGorge && potentialTarget.getEntity().getEdict()->v.team == ALIEN_TEAM) {
                setTarget(&potentialTarget);
                
            } else if (targetIsMine && (vecToTarget.Length() > MIN_MINE_RANGE) &&
                (((evolution == kFade) && (HiveManager::getActiveHiveCount() == 3)) || (evolution == kGorge)))
			{
				// kill the damn mine, since we're not too close and we'll prob. step on it otherwise
				setTarget(&potentialTarget);
				
			} else if (_bot.getTarget() == NULL && !targetIsMine) {
				// if we've got no target, lock onto this one
				setTarget(&potentialTarget);
				
			} else if (_bot.getTarget() == &potentialTarget) {
				// ok, we have a target.  just update our last seen time.
				
			} else if (((gpGlobals->time > _lastTargetSeenTime + TARGET_TIMEOUT) || (vecToTarget.Length() < MELEE_RANGE))
                && !targetIsMine) 
            {
				// there's a new higher priority target, and we haven't seen our target in a while or the high priority target is
                // really close
				setTarget(&potentialTarget);
				
			} else if (_bot.getTarget() != NULL && 
				_bot.getTarget()->getInfo() != NULL && 
				_bot.getTarget()->getInfo()->isBuilding() &&
				potentialTarget.getInfo()->getType() == kPlayer)
			{
				// we were attacking a building, and a player just showed up.  let's kill them first.
				setTarget(&potentialTarget);
			}
			
			if (_bot.getTarget() != NULL) {
				_lastTargetSeenTime = gpGlobals->time;
			}
			return true;
		}
    }
    return false;
}


void CombatNavMethod::pause()
{
	//NOP
}

void CombatNavMethod::resume()
{
	//NOP
}

void CombatNavMethod::setTarget(Target* pNewTarget)
{
	_bot.setTarget(pNewTarget);

	if (pNewTarget != NULL && !pNewTarget->getEntity().isNull()) {
        _targetName = STRING(pNewTarget->getEntity().getEdict()->v.classname);
        _targetLoc = pNewTarget->getEntity().getEdict()->v.origin;
    }
}


// FIXME: CONFIG
const float CLOSE_RANGE = 64.0;
const float JUMP_PERIOD = 0.3;
const int JUMP_CHANCE_PERCENT = 50;

const int STRAFE_MAX_SPEED = 1000;
const float EVASION_ANGLE = 45;

void CombatNavMethod::evade(float range)
{
    float closeRange = CLOSE_RANGE;
    
    // if we're not close and not cloaked, let's jump around.  Only skulks and gorges do this, however
    if (_bot.getSensor()->threatSeen()) {
        if (range > closeRange && gpGlobals->time > _lastJumpTime + JUMP_PERIOD &&
            (!_bot.isCloaked() && (_bot.getEvolution() == kSkulk || _bot.getEvolution() == kGorge || _bot.getEvolution() == kLerk)))
        {
            if (RANDOM_LONG(0, 100) > JUMP_CHANCE_PERCENT) {
                _bot.getEdict()->v.button |= IN_JUMP;
            }
            _lastJumpTime = gpGlobals->time;
    
            _strafeSpeed = RANDOM_LONG(STRAFE_MAX_SPEED/2, STRAFE_MAX_SPEED);
            if (RANDOM_LONG(0, 100) > 50) {
                _strafeSpeed *= -1;
            }
        }
        
        if (_strafeSpeed != 0) {
            

            if (range > closeRange * 2) {
                // Rotate the target vector so that the bot doesn't run up to them in a straight line
                Vector relativeVector(_bot.getTarget()->getOrigin() - _bot.getEdict()->v.origin);
                relativeVector = relativeVector.Normalize();
                float rotationAmount = EVASION_ANGLE * M_PI / 180.0;
                if (_strafeSpeed > 0) {
                    rotationAmount *= -1;
                }
                
                float cosRotation = cos(rotationAmount);
                float sinRotation = sin(rotationAmount);
                Vector rotatedVector(cosRotation * relativeVector.x - sinRotation * relativeVector.y,
                    sinRotation * relativeVector.x + cosRotation * relativeVector.y,
                    relativeVector.z);
                rotatedVector *= 100;
                Vector evasionTarget(_bot.getEdict()->v.origin + rotatedVector);
                
                //WaypointDebugger::drawDebugBeam(_bot.getEdict()->v.origin, evasionTarget, 255, 0, 0);
                _bot.getMovement()->setTarget(evasionTarget, 30.0);
				_bot.getMovement()->strafe(_strafeSpeed);
            }
        }
    }
}


Vector CombatNavMethod::calculateInterceptSolution(const Vector& marinePos, const Vector& alienPos, 
                                                   const Vector& marineVelocityVec, float alienSpeed)
{
    Vector posOffset = marinePos - alienPos;
    float range = posOffset.Length();
    float marineSpeed = marineVelocityVec.Length();
    // if the marine is stationary, we're really really close, the alien is stationary or the
    // range is fairly long, just go towards where they are right now
    if (marineSpeed < 10 || range < 10 || alienSpeed < 10 || range > 500) {
        return posOffset;
    }

    // otherwise let's aim for where they're going to be in a moment
    float currentInterceptTime = range / alienSpeed;
    return posOffset + marineVelocityVec * (currentInterceptTime);
}


std::string CombatNavMethod::getName() const
{
	return "CombatNavigation";
}


tBotWeaponSelect gWeaponSelect[] = { 
    // min distance, max distance, min hives, min fire delay, min energy, fire_time, purpose, evolution
    //                                             mind  maxd hiv fdel eng  fire, purpose
	{9999,							"",			   0.0,   0.0, 4, 0.0,  0, 0.0,           0,kInvalidEvolution}, //tmc - need to make selectweapon work correctly, don't know why.

   {WEAPON_DIVINEWIND,   "weapon_divinewind",      0.0, 400.0, 3, 3.0, 70, 0.0, ANTI_PLAYER, kSkulk},
   {WEAPON_BITE,         "weapon_bitegun",         0.0,  60.0, 0, 0.1,  5, 0.0, MELEE_ATTACK | ANTI_PLAYER | ANTI_BUILDING, kSkulk}, 
   {WEAPON_PARASITE,     "weapon_parasite",        0.0, 500.0, 1, 1.0, 30, 0.0, ANTI_PLAYER, kSkulk}, 
   {WEAPON_LEAP,         "weapon_leap",          128.0, 512.0, 2, 2.0, 25, 0.3, ANTI_PLAYER | ANTI_BUILDING, kSkulk}, 

   {WEAPON_WEBSPINNER,	  "weapon_webspinner",		0.0, 999.0, 3, 0.1, 18, 0.0, ANTI_PLAYER, kGorge},
   {WEAPON_BILEBOMB,      "weapon_bilebombgun",     0.0, 350.0, 2, 0.7, 22, 0.0, ANTI_BUILDING, kGorge}, 
   {WEAPON_HEALINGSPRAY,  "weapon_healingspray",    0.0, 300.0, 1, 0.5, 15, 1.0, ANTI_PLAYER | HELPS_FRIENDLY, kGorge},
   {WEAPON_SPIT,          "weapon_spit",            0.0, 999.0, 0, 0.3, 12, 0.0, MELEE_ATTACK | ANTI_PLAYER, kGorge}, 

   {WEAPON_UMBRA,         "weapon_umbra",           0.0, 999.0, 2, 2.0, 30, 0.2, ANTI_PLAYER, kLerk}, 
   {WEAPON_PRIMALSCREAM,  "weapon_primalscream",    0.0, 999.0, 3, 3.0, 45, 0.2, ANTI_PLAYER | ANTI_BUILDING, kLerk}, 
   {WEAPON_SPORES,        "weapon_spore",           0.0,1500.0, 1, 1.0, 35, 0.2, ANTI_PLAYER, kLerk}, 
   {WEAPON_BITE2,         "weapon_bite2gun",        0.0,  60.0, 0, 0.2,  5, 0.0, MELEE_ATTACK | ANTI_PLAYER | ANTI_BUILDING, kLerk}, 
 
   {WEAPON_ACIDROCKET,    "weapon_acidrocketgun", 120.0,1500.0, 3, 0.5, 25, 0.2, ANTI_PLAYER | ANTI_BUILDING, kFade}, 
   {WEAPON_SWIPE,         "weapon_swipe",           0.0, 120.0, 0, 0.1,  6, 0.0, MELEE_ATTACK | ANTI_PLAYER | ANTI_BUILDING, kFade}, 
   {WEAPON_BLINK,         "weapon_blink",         120.0,1500.0, 1, 0.4,  7, 0.1, ANTI_PLAYER | ANTI_BUILDING, kFade}, 
   {WEAPON_METABOLIZE,    "weapon_metabolize",    0.0,   999.0, 2, 5.0, 25, 1.0, 0, kFade}, 
 
   {WEAPON_CHARGE,		  "weapon_charge",		   50.0, 999.0, 3, 0.0, 40, 0.3, ANTI_PLAYER | ANTI_BUILDING, kOnos},
   {WEAPON_STOMP,         "weapon_stomp",         100.0, 650.0, 2, 2.0, 30, 0.3, ANTI_PLAYER, kOnos}, 
   {WEAPON_DEVOUR,        "weapon_devour",         55.0, 110.0, 1, 5.0, 20, 0.8, MELEE_ATTACK | ANTI_PLAYER, kOnos}, 
   {WEAPON_CLAWS,         "weapon_claws",           0.0, 128.0, 0, 0.0,  7, 0.0, MELEE_ATTACK | ANTI_PLAYER | ANTI_BUILDING, kOnos}, 

   /* terminator */ 
   {0,                    "",                       0.0,   0.0, 0, 0.0,  0, 0.0, 0, kInvalidEvolution} 
}; 
