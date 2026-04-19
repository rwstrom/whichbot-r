//
// $Id: Bot.cpp,v 1.61 2008/03/08 06:05:21 masked_carrot Exp $

// Copyright (c) 2003, WhichBot project
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

#include<cstring>

#include "Bot.h"
#include "NSConstants.h"
#include "extern/halflifesdk/extdll.h"
#include "extern/halflifesdk/util.h"
#include "extern/metamod/meta_api.h"
#include "combat/CombatNavMethod.h"
#include "navigation/WaypointNavMethod.h"
#include "worldstate/HiveManager.h"
#include "strategy/HiveMind.h"
#include "BotManager.h"
#include "worldstate/WorldStateUtil.h"
#include "config/Config.h"
#include "config/TranslationManager.h"
#include "strategy/FleeStrategy.h"


Log Bot::_log(__FILE__);

const float DEFAULT_REWARD_FACTOR = 1.0;

// Sometimes it doesn't seem to change weapon, for some reason (maybe timeouts
// stopped us from switching)
const float WEAPON_CHANGE_TIME = 0.3;

bool pathValidator(void* pRock, int upstreamNodeId, int thisNodeId)
{
	WaypointManager* pWptMgr = (WaypointManager*)pRock;
	return pWptMgr->isPathValid(upstreamNodeId, thisNodeId);
}

Bot::Bot(int botIdx) :
	_entity(NULL),
	_pathMgr(DEFAULT_REWARD_FACTOR),
	_stratMgr(),
	_statusReporter(),
	_pName(NULL),
	_pSensor(NULL),
	_pNavEngine(NULL),
	_pMovement(NULL),
	_pTarget(NULL),
	_evolution(kSkulk),
	_desiredEvolution(kSkulk),
	_lastEvolution(0),
	_currentWeaponId(-1),
	_maxArmour(0),
	_role(Config::getInstance().getBotRole(botIdx)),
	_dead(true)
{
	//empty
	//setProperty(kLastLifeTime, 100);
	// path validators disabled till they work properly
	//_pathMgr.setPathValidatorFn(pathValidator, &gpBotManager->getWaypointManager());
}


Bot::~Bot()
{
    delete _pName;
    _pName = NULL;
    delete _pSensor;
    _pSensor = NULL;
    delete _pNavEngine;
    _pNavEngine = NULL;
    delete _pMovement;
    _pMovement = NULL;
	if (_pTarget)
		delete _pTarget;
    _pTarget = NULL;
}


//
// Processed every TICK_TIME.  Does all the reasoning, etc. for this bot.
//
int Bot::tick()
{
    
    if (!_entity.isNull()) {
		gpCurrentThinkingBot = this;
        
		if ((_entity.getEdict()->v.iuser4 & MASK_ALIEN_EMBRYO))
		{
            evolve();
			_currentWeaponId = 0;
			return 0;

        } else {
            bool normalMove = false;
            
            // sanity check - sometimes we can become another evolution unexpectedly
            if (_evolution != EVOLUTIONS[_entity.getEdict()->v.iuser3] &&
                _entity.getEdict()->v.iuser3 >= PLAYERCLASS_ALIVE_LEVEL1 &&
                _entity.getEdict()->v.iuser3 <= PLAYERCLASS_ALIVE_LEVEL5)
            {
                _evolution = EVOLUTIONS[_entity.getEdict()->v.iuser3];
            }

            if (_entity.getEdict()->v.team == NO_TEAM) {
                if ((_entity.getEdict()->v.flags & FL_SPECTATOR) != 0) {
                    // Uh-oh, we blundered into the spectator entrance somehow
                    readyroom();
                    
                } else {
                    startGame();
                }
                
            } else if (_entity.getEdict()->v.team == MARINE_TEAM) {
                readyroom();
                
            } else {
                
                // if the bot is dead, we need to respawn
                if ((_entity.getHealth() < 1) || (_entity.getEdict()->v.deadflag != DEAD_NO)) {
                    if (!_dead) {
						float lastLifetime = gpGlobals->time - getProperty(kLastSpawnTime,0);
                        respawnBot();
						setProperty(kLastLifeTime,lastLifetime);
                        _pSensor->clear();
                    }
                    _dead = true;
                    
                } else {
                    bool targetSeen = _pSensor->targetSeen();
                    bool threatSeen = _pSensor->threatSeen();
                    if (_dead) {
						targetSeen = false;
						threatSeen = false;
                        // force the sensor to scan, we just spawned and someone might be trying to kill us
                        _pSensor->scan(true);
                        
                    } else {
                        _pSensor->scan();
                    }
                    _dead = false;
                    
                    // Let's make it so gorges don't attack buildings unless they have 2 hives
                    int numHives = HiveManager::getActiveHiveCount();
					
                    bool newTargetSeen = 
                        (!targetSeen && _pSensor->targetSeen()) || 
                        (!threatSeen && _pSensor->threatSeen());
                    
                    if (newTargetSeen &&
                        (_evolution != kGorge || numHives >= 2 || _pSensor->threatSeen() || _pSensor->healableFriendSeen()))
                    {
                        // ok, we see a new target.  Let's switch into combat mode
                        _pNavEngine->setNextMethod(new CombatNavMethod(*this));
                    }

                    if (_entity.getEdict()->v.armorvalue > _maxArmour) {
                        _maxArmour = _entity.getEdict()->v.armorvalue;
                    }
                    
                    
					checkCheating();
                    checkEvolution();
					checkPackEvolution();
                    normalMove = true;				
                }
            }
            
            if (normalMove && _pNavEngine != NULL) {
                if (gpGlobals->time < getProperty(kWeaponFireTillTime)) {
					if (_currentWeaponId == WEAPON_CHARGE)
						_entity.getEdict()->v.button |= 2048; // movement key
					else
						_entity.getEdict()->v.button |= IN_ATTACK;
					//setWeaponImpulse();
                    
                } else {
                    setProperty(kWeaponFireTillTime, 0.0);
                }
                _pNavEngine->navigate();
                
            } else {
                if (_pMovement != NULL) {
                    _pMovement->nullMove();
                }
            }
        }
    }
	
	setProperty(kLastFrameTime);
	return 0;
}


void Bot::evolve()
{
    if (_lastEvolution < FORM_CHANGE) {
        if (!hasTrait(_lastEvolution)) {
            _traits.push_back(_lastEvolution);
        }
        
    } else {
        int lastEvolutionImpulse = _lastEvolution - FORM_CHANGE;
        _evolution = kSkulk;
        for (int ii = 0; ii < NUM_EVOLUTIONS; ii++) {
            if (lastEvolutionImpulse == Config::getInstance().getImpulse(EVOLUTION_IMPULSES[ii])) {
                _evolution = (tEvolution)ii;
            }
        }
    }
    if (_pMovement != NULL) {
        // we can't do anything right now, so just wait till we hatch
        _pMovement->nullMove();
    }
}

void Bot::userInfoChanged(const char* infoBuffer)
{
	int infoBufLen = strlen(infoBuffer);
	//if (strstr(infoBuffer, "model\\gestate") != NULL) {

	//}
	
	if (strstr(infoBuffer, "undefinedteam")) {
		_dead = true;
		resetBot();
		
	} else if (_dead) {
		_dead = false;
		setProperty(kLastSpawnTime);
	}
	
	const char* namePos = std::strstr(infoBuffer, "name\\");
	if (namePos != NULL) {	
		namePos += strlen("name\\");
		const char* nameEnd = namePos;
		for (; (nameEnd - infoBuffer) < infoBufLen; nameEnd++) {
			if (*nameEnd == '\\') {
				break;
			}
		}
		char namebuf[128];
		strncpy(namebuf, namePos, nameEnd - namePos);
		namebuf[nameEnd - namePos] = '\0';
		delete _pName;
		_pName = new std::string(namebuf);
	}
}


void Bot::selectDesiredEvolution()
{
	_desiredEvolution = HiveMind::getDesiredEvolution(*this);
}


void Bot::checkCheating()
{
	// If cheats are enabled, let's cheat!
	if (gpBotManager->areCheatsEnabled() && 
		((gpBotManager->inCombatMode()) || getResources() < 100) &&
		gpGlobals->time > getProperty(kLastCheatCheckTime) + 1.0)
	{
        giveCheatPoints();

		setProperty(kLastCheatCheckTime);
	}
}


void Bot::giveCheatPoints()
{
	if (gpBotManager->inCombatMode()) {
		fakeClientCommand("givexp");
		
	} else {
		fakeClientCommand("givepoints");
	}
}


void Bot::checkEvolution()
{
	// Only evolve if we can't see an enemy
	if (_pSensor && !_pSensor->targetSeen()) {
		// and only check for evolution every so often and not if we died immediately last spawn
		if ((gpGlobals->time > getProperty(kLastEvolveCheckTime) + 1.0)/* && (getProperty(kLastLifeTime) > 30.0)*/)
		{
			bool evolutionShift = false;

			if (_evolution != _desiredEvolution) {				
				tNodeId nearestId = gpBotManager->getWaypointManager().
					getNearestWaypoint(MIN_WALK_EVOLUTION, *(_entity.getOrigin()), _entity.getEdict());
				
				if (nearestId >= 0) {
					// Only evolve lifeform if we're near a walkable waypoint
					if ((gpBotManager->getWaypointManager().getFlags(nearestId) & W_FL_WALKABLE) != 0) {
						int impulse = 
							gpBotManager->inCombatMode() ? 
							getCombatEvolutionImpulse() : 
                            Config::getInstance().getImpulse(EVOLUTION_IMPULSES[_desiredEvolution]);

						if (impulse != 0) {
							_entity.getEdict()->v.impulse = impulse;
							_lastEvolution = FORM_CHANGE + impulse;
							evolutionShift = true;
						}
					}
				}				
			}
			
			// If we didn't change lifeforms, check for upgrade evolutions
			if (!evolutionShift) {
                int desiredUpgradeImpulse = HiveMind::getDesiredUpgrade(*this);
                if (desiredUpgradeImpulse != 0) {
                    _entity.getEdict()->v.impulse = desiredUpgradeImpulse;
                    _lastEvolution = desiredUpgradeImpulse;
                }
			}
			
			setProperty(kLastEvolveCheckTime);
		}
	}
}


byte Bot::getCombatEvolutionImpulse()
{
    byte impulse = 0;
    const int defaultMinResources[] = { 0, 480, 300, 720, 720 };
	
	int resources = getResources();
	int minResources = (int)Config::getInstance().getArrayTweak("game/resources/combat_min_resources", _desiredEvolution, defaultMinResources[_desiredEvolution]);
	if (_desiredEvolution != kSkulk) {
		if (resources >= minResources) {
	        impulse = Config::getInstance().getImpulse(EVOLUTION_IMPULSES[_desiredEvolution]);
		} else {
			// if we don't have a better idea, let's lerk in the meantime
			if (!gpBotManager->areCheatsEnabled() && resources >= Config::getInstance().getArrayTweak("game/resources/combat_min_resources", kLerk, defaultMinResources[kLerk])) {
				return Config::getInstance().getImpulse(EVOLUTION_IMPULSES[kLerk]);
			}
		}
    }
    return impulse;
}


int botNameIdx = 0;
const char* botNames[] = {
	"[wb]WhichOne", "[wb]Who", "[wb]What", "[wb]That", "[wb]It", "[wb]He", "[wb]They", "[wb]TheOther", NULL
};

const char* gorgeName = "[wb]TheFatOne";

const char* getNewBotName()
{
	static std::string newName = botNames[botNameIdx];
	int numConfiguredNames = Config::getInstance().getArraySize("admin/bot_names");
	//std::cout << "NUM CONFIG NAMES " << numConfiguredNames << std::endl; 
	if (numConfiguredNames > 0) {
		int nameIdx = botNameIdx++ % numConfiguredNames;
		newName = Config::getInstance().getArrayTweak("admin/bot_names", nameIdx, "[wb]Bot");
		return newName.c_str();//Config::getInstance().getArrayTweak("admin/bot_names", nameIdx, "[wb]Bot");

	} else {

		if (gpBotManager->getNumBots() == 1) {
			return gorgeName;
		}
		
		const char* name = botNames[botNameIdx];
		if (name == NULL) {
			botNameIdx = 0;
			name = botNames[0];
		}
		botNameIdx++;
		return name;
	}
}


bool Bot::create()
{
	delete _pName;
	_pName = new std::string(getNewBotName());
	_entity = WorldStateUtil::createPlayer(*_pName);
	if (!_entity.isNull()) {
        delete _pMovement;
        _pMovement = new BotMovement(*this);
		resetBot();
		return true;
	}
	return false;
}


int Bot::resetBot()
{
	HiveMind::unsetStrategies(*this);
	_properties.clear();

	// If we're in classic mode we lose all our upgrades when we die
	if (!gpBotManager->inCombatMode())
	{
		_traits.clear();
	}

	if (_pNavEngine == NULL) {
		_pNavEngine = new NavigationEngine(*this);
	}

	if (_pSensor == NULL) {
		_pSensor = new BotSensor(*this);
	}

	// Make sure we always set the nav method just in case we're in another type of nav
	// before (like combat nav).  Also, this allows us to clear out the history and any
	// wpt state information that the previous nav method may be holding which could
	// cause the bot to try to go to a wpt that's much too far away at the time the bot
	// was reseted.
	_pNavEngine->setNextMethod(new WaypointNavMethod(*this, true));
	

	_evolution = kSkulk;
    
	setProperty(kLastSpawnTime);

	selectDesiredEvolution();
	

	HiveMind::setStrategies(*this);
    _lastWeaponFireTimes.clear();
    _lastWeaponFireTimes.resize(WEAPON_MAX+1);
    _maxArmour = 0;
    setProperty(kWeaponFireTillTime, 0);
    _buildingBlacklist.clear();
	_currentWeaponId =0;

    for (std::vector<float>::iterator ii = _lastWeaponFireTimes.begin(); ii != _lastWeaponFireTimes.end(); ii++) {
        *ii = 0.0f;
    }
	_entity.getEdict()->v.flags |= FL_FAKECLIENT;

    return 0;
}


void Bot::startGame()
{
	WorldStateUtil::joinTeam(_entity.getEdict(), ALIEN_TEAM);
	
	if (_entity.getTeam() == ALIEN_TEAM)
	{
		if (_pSensor) _pSensor->scan(true);
		setProperty(kLastSpawnTime);
		setProperty(kLastLifeTime,99);

	}
}


void Bot::selectItem(const std::string& item_name)
{
	fakeClientCommand(item_name);
}


extern std::vector<std::string> g_fakeArgs;
void Bot::fakeClientCommand(const std::string& cmd)
{
	// allow the MOD DLL to execute the ClientCommand...
	g_fakeArgs.push_back(cmd);
	MDLL_ClientCommand(_entity.getEdict());
	g_fakeArgs.clear();
}


void Bot::respawnBot()
{
	resetBot();
	// TODO
	
	setProperty(kLastSpawnTime);
}


void Bot::sayToTeam(const std::string& msg)
{
	if (gpBotManager->isBotChatEnabled()) {
		g_fakeArgs.push_back("say_team");
		fakeClientCommand(msg);
	}
}


tEvolution Bot::getEvolution(edict_t* pEdict)
{
    if (pEdict != NULL) {
        switch (pEdict->v.iuser3) {
        case PLAYERCLASS_ALIVE_LEVEL1:
            return kSkulk;
        case PLAYERCLASS_ALIVE_LEVEL2:
            return kGorge;
        case PLAYERCLASS_ALIVE_LEVEL3:
            return kLerk;
        case PLAYERCLASS_ALIVE_LEVEL4:
            return kFade;
        case PLAYERCLASS_ALIVE_LEVEL5:
            return kOnos;
        }
    }
    // if they're currently evolving, we're going to say they're a skulk.  at some point perhaps we should
    // retrofit to cope with them being an egg.
    return kSkulk;
}


bool Bot::isEvolving(edict_t* pEdict)
{
    return pEdict != NULL ? (pEdict->v.iuser4 & MASK_ALIEN_EMBRYO)!= 0 : false;
}


Vector Bot::getGroundLevelOrigin ()
{
	return getGroundLevelOrigin(_entity.getEdict(), _evolution);
}


Vector Bot::getGroundLevelOrigin (const edict_t* pEdict, tEvolution evolution)
{
	if(evolution < kSkulk || evolution > kOnos ){
		
		evolution = kSkulk;
	}
	return pEdict->v.origin - Vector(0, 0, g_CreatureOriginHeights[evolution]);
}


bool Bot::selectWeapon(Target* pTarget, float range, bool shouldUseMeleeAttack)
{
	if (!pTarget){
		if (_pTarget && _pTarget->isValid())
			pTarget = _pTarget;
		else 
			return false;
	}
	if (range > 125)
		shouldUseMeleeAttack = false;
	else
		shouldUseMeleeAttack = true;

	int wpnIdx = 0;

    // Some weapons take time to fire, because we jump first
    int activeHiveCount = 1;        
    if (gpBotManager->inCombatMode()) {
        if (hasTrait(Config::getInstance().getImpulse(IMPULSE_HIVE2))) {
            activeHiveCount++;
        }
        if (hasTrait(Config::getInstance().getImpulse(IMPULSE_HIVE3))) {
            activeHiveCount++;
        }
    } else {
        activeHiveCount = HiveManager::getActiveHiveCount();
    }


	if (_evolution == kGorge && activeHiveCount > 1 && pTarget->getInfo()->isBuilding())
	{
		selectWeapon(WEAPON_BILEBOMB);
		return true;
	}

	while (gWeaponSelect[wpnIdx].iId > 0) {
        tBotWeaponSelect& wpn = gWeaponSelect[wpnIdx];
		
		bool selectThisWpn = false;
		// TODO - check energy status
		if ((_evolution == wpn.evolution) &&
			(activeHiveCount >= wpn.min_hives))
		{
			// if we're using a melee attack, we're not attacking the target, just biting something on the way
            if ((pTarget->getEntity().getEdict()->v.team == ALIEN_TEAM) && ((wpn.purpose & HELPS_FRIENDLY) != 0)) {
                selectThisWpn = true;

            } else if (shouldUseMeleeAttack && ((wpn.purpose & MELEE_ATTACK) != 0)) {
				if(_evolution == kOnos && (wpn.iId ==WEAPON_DEVOUR) && (pTarget->getInfo()->isBuilding()|| ( _entity.getEdict()->v.iuser4 & MASK_DIGESTING)) )
					selectThisWpn = false;
				else
					selectThisWpn = true;
				
			}else if((_evolution == kOnos && wpn.iId == WEAPON_STOMP) && (pTarget->getEntity().getEdict()->v.team == MARINE_TEAM && pTarget->getEntity().getEdict()->v.iuser4 & MASK_UPGRADE_7)){
				selectThisWpn = false; //Don't stomp jp
			}else if ((_evolution == kLerk && wpn.iId == WEAPON_SPORES) && (pTarget->getEntity().getEdict()->v.team == MARINE_TEAM && (pTarget->getEntity().getEdict()->v.iuser4 & MASK_UPGRADE_13) !=0)){
				selectThisWpn = false; //Don't spore Heavy armor
			} else if ((range >= wpn.min_distance) && (range <= wpn.max_distance)) {
				
				if ((pTarget == NULL) ||
					(pTarget->getInfo()->isBuilding() && ((wpn.purpose & ANTI_BUILDING) != 0)) ||
					(!pTarget->getInfo()->isBuilding() && ((wpn.purpose & ANTI_PLAYER) != 0) && pTarget->getEntity().getEdict()->v.team != ALIEN_TEAM))
				{
                    //int sz = _lastWeaponFireTimes.size();
                    float lastTimeWeaponFired = _lastWeaponFireTimes[wpn.iId];
                    if (gpGlobals->time > lastTimeWeaponFired + wpn.min_fire_delay) {
                        float currentEnergy = getEnergy();
                        if (currentEnergy >= wpn.min_energy) {
                            //_log.Debug("Using weapon %s", wpn.weapon_name);
        					selectThisWpn = true;
                        }
                    }
				}
			}
		}
		
		if (selectThisWpn) {
			//LOG_CONSOLE(PLID,"using %s",wpn.weapon_name );
            selectWeapon(wpn.iId);
			return true;
		}
		
		wpnIdx++;
	}
	
	return false;
}


bool Bot::selectWeapon(int wpnId)
{
	if (_currentWeaponId == wpnId && (_currentWeaponId > 0 && _currentWeaponId < WEAPON_MAX))
	{
		return true;
	}
	if (wpnId == WEAPON_CHARGE)
	{
		_currentWeaponId = wpnId;
		return true;
	}
    int wpnIdx = 0;
    tBotWeaponSelect& wpn = gWeaponSelect[0];
	while (wpn.iId != wpnId && wpn.iId > 0) {
        wpn = gWeaponSelect[++wpnIdx];
    }

    if (gpGlobals->time > getProperty(kLastWeaponChangeTime) + WEAPON_CHANGE_TIME)
    {
        //_log.Debug("Selected weapon %s", wpn.weapon_name);
        selectItem(wpn.weapon_name);
        _currentWeaponId = wpn.iId;
        setProperty(kLastWeaponChangeTime);
        return true;
    }
    return false;
}


void Bot::fireWeapon()
{
	if (_currentWeaponId > 0) {
		int wpnIdx = 0;
		tBotWeaponSelect& wpn = gWeaponSelect[0];
		while (wpn.iId != _currentWeaponId && wpn.iId > 0) {
			wpn = gWeaponSelect[++wpnIdx];
		}
    
		if (!_entity.isNull()) {
			_entity.getEdict()->v.button &= ~2048; // movement and attack keys can't be used together
            _lastWeaponFireTimes[_currentWeaponId] = gpGlobals->time;
			_entity.getEdict()->v.button |= IN_ATTACK;
			setProperty(kWeaponFireTillTime,
            (wpn.fire_time != 0) ? wpn.fire_time + gpGlobals->time : 0);
			setWeaponImpulse();
		}
	}
}


void Bot::setWeaponImpulse()
{
	
    switch (_currentWeaponId) {
    case WEAPON_LEAP:
        _entity.getEdict()->v.impulse = Config::getInstance().getImpulse(IMPULSE_LEAP);
        break;
    case WEAPON_BLINK:
        _entity.getEdict()->v.impulse = Config::getInstance().getImpulse(IMPULSE_BLINK);
        break;
    case WEAPON_CHARGE:
        //_entity.getEdict()->v.impulse = Config::getInstance().getImpulse(IMPULSE_CHARGE);
        _entity.getEdict()->v.button |= 2048;
		_entity.getEdict()->v.button &= ~IN_ATTACK; // movement and attack keys can't be used together
		break;
    default:
        break;
    }
}


void Bot::kill()
{
	if (!_entity.isNull()) {
		MDLL_ClientKill(_entity.getEdict());
	}
}


short Bot::getResources()
{
	if (!_entity.isNull()) {
		return (short)(_entity.getEdict()->v.vuser4.z / 100);

	} else {
		return 0;
	}
}


bool Bot::isCloaked()
{
	if (!_entity.isNull()) {
		//True if sensory chamber is nearby or we have cloaking upgrade and we haven't been sighted.
		return ((_entity.getEdict()->v.iuser4 & (MASK_ALIEN_CLOAKED|MASK_UPGRADE_7)) != 0u  && (_entity.getEdict()->v.iuser4 &MASK_VIS_SIGHTED)== 0u);
	} else {
		return false;
	}
}


Target* Bot::getTarget()
{ 
    if ((_pTarget != NULL) && _pTarget->getEntity().isNull()) {
        setTarget(NULL);
    }
    return _pTarget;
}


void Bot::setTarget(Target* newTarget)
{ 
    if (_pTarget != NULL) {
        delete _pTarget;
    }
    if (newTarget == NULL) {
        _pTarget = NULL;

    } else {
        _pTarget = new Target(*newTarget);
    }
}


bool Bot::needsHealing()
{
    return (!_entity.isNull() && 
            (_entity.getEdict()->v.health < _entity.getEdict()->v.max_health ||
            _entity.getEdict()->v.armorvalue < WorldStateUtil::getMaxArmour(_entity.getEdict())));
}


void Bot::failedToBuildAt(tNodeId waypointId)
{
    if (find(_buildingBlacklist.begin(), _buildingBlacklist.end(), waypointId) == _buildingBlacklist.end()) {
        _buildingBlacklist.push_back(waypointId);
    }
}


bool Bot::hasFailedToBuildAt(tNodeId waypointId)
{
    return find(_buildingBlacklist.begin(), _buildingBlacklist.end(), waypointId) != _buildingBlacklist.end();
}


void Bot::generateStatusReport()
{
	_statusReporter.generateStatusReport(*this);
}


float Bot::getProperty(eBotProperty property, float defaultVal)
{
	tBotPropertyMap::iterator found = _properties.find((int)property);
	if (found != _properties.end()) {
		return found->second;

    } else { 
		return defaultVal;
	}
}


void Bot::setProperty(eBotProperty property, float value)
{
	_properties[(int)property] = value;
}


void Bot::checkPackEvolution()
{
	PackInfo* pInfo = HiveMind::getPackManager().getPackInfo(*this);
	if (pInfo != NULL) {
		Bot* pLeader = pInfo->getLeader();
		if (pLeader != NULL && this != pLeader && _evolution > pLeader->getEvolution()) {
			_log.Debug("Bot evolving into pack leader");
			HiveMind::getPackManager().promoteBot(*this);
		}
	}
}


void Bot::handleDamage(byte armourDamage, byte healthDamage, [[maybe_unused]] const Vector& dmgOrigin)
{
	/* Apparently this gets called even when the bots is in the middle of a tick().
	* Since this adds to the botSensor vector<target>, bad things happen if the
	* bot happens to be iterating through that vector at the time.
	* Disable this call by commenting out registerHandle call in NetMessageHandlers.
	*/

	_log.Debug("Handling damage [armour=%d, health=%d]", armourDamage, healthDamage);
	// ok, let's see if we can find the damage dealer
	if(_entity.getEdict()->v.dmg_inflictor && _entity.getEdict()->v.dmg_inflictor->v.owner)
	{
		if (WorldStateUtil::isPlayerPresent(_entity.getEdict()->v.dmg_inflictor->v.owner))
		{
			if(_pSensor)
			{
				_pSensor->forceScanEntity(_entity.getEdict()->v.dmg_inflictor->v.owner);

			}
		}
	}//tmc
	/*
	for (int ii = 1; ii <= gpGlobals->maxClients; ii++) {
		CBaseEntity* pPlayerBaseEntity = UTIL_PlayerByIndex(ii);

		if (pPlayerBaseEntity != NULL) {
			edict_t* pPlayerEdict = INDEXENT(ii);
			
            if (WorldStateUtil::isPlayerPresent(pPlayerEdict) && (pPlayerEdict->v.skin >= 0)) {
				float range = (pPlayerEdict->v.origin - dmgOrigin).Length();
				if (range < 10) {
					// consider this the damage dealer, we'll short cut and stop here
					_pSensor->forceScanEntity(pPlayerEdict);
					break;
				}
			}
		}
	}*/
}


bool Bot::isInjured()
{
	if (_entity.isNull())
		return false;
	else
		return _entity.getEdict()->v.health < _entity.getEdict()->v.max_health ||
		_entity.getEdict()->v.armorvalue < WorldStateUtil::getMaxArmour(_entity.getEdict());
}

bool Bot::isCharging()
{
	if (_entity.isNull())
		return false;
	else
		return ((_entity.getEdict()->v.iuser4 & MASK_ALIEN_MOVEMENT)!=0);
}
