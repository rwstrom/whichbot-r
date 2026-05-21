//
// $Id: NetMessageHandlers.cpp,v 1.20 2008/03/06 21:18:31 masked_carrot Exp $

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

#include "message/NetMessageDispatcher.h"
#include "framework/Log.h"
#include "worldstate/AreaManager.h"
#include "worldstate/HiveManager.h"
#include "strategy/HiveMind.h"
#include "BotManager.h"

void handleNetAreaInfoMsg(const NetMessage& msg)
{
    if (msg.size() == 6) {
        const char* areaName = msg.getCStringAt(1);
		// Let's not add the same area more than once.
        if (areaName != NULL) {
    		if (!AreaManager::exists(areaName)) {
	    		Vector bottomLeft(msg.getFloatAt(2), msg.getFloatAt(3), -100000);
				Vector topRight(msg.getFloatAt(4), msg.getFloatAt(5), 100000);
				AreaInfo info(areaName, bottomLeft, topRight);
		    	AreaManager::addArea(info);
				WB_LOG_DEBUG("Added area {}", areaName);
    		}
        }
    }
}


/****
NS 3.1 sample hive status data

Preamble
[0]kByte=0x03

Hive 1

[1]kByte=0x07
[2]kFloat=2000.000000 - Hive position x
[3]kFloat=2224.000000 - Hive position y
[4]kFloat=-48.000000 - Hive position z
[5]kByte=0x06 - Hive Health
[6]kByte=0x64

Hive 2
[7]kByte=0x07
[8]kFloat=3240.000000 - Hive position x
[9]kFloat=-464.000000 - Hive position y
[10]kFloat=-32.000000 - Hive position z
[11]kByte=0x00 - Hive Health
[12]kByte=0x64

Hive 3
[13]kByte=0x07
[14]kFloat=-1285.000000 - Hive position x
[15]kFloat=2497.000000 - Hive position y
[16]kFloat=-149.000000 - Hive position z
[17]kByte=0x00 - Hive Health
[18]kByte=0x64

****/

bool isNS33Version()
{
	static cvar_t* nsversion = CVAR_GET_POINTER("sv_nsversion");
	return (nsversion != NULL);
}

edict_t* getHiveEntity (Vector& position)
{
	CBaseEntity* pEntity = NULL;
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, position, 300)) != NULL) {
		if (strcmp(STRING(pEntity->edict()->v.classname), "team_hive") == 0) {
			// tmc - Some siege style maps have the hives really close together.
			// So I added this line to make sure we get the right one.
			if (pEntity->edict() &&pEntity->edict()->v.origin == position)
				return pEntity->edict();
		}
	}
	WB_LOG_ERROR("ERROR: no hive found at given position");
	return NULL;
}


void handleTextDisplayMsg(const NetMessage& msg)
{
    if (msg.size() == 1) {
        const char* expectedText = "GameStarting";
        const char* text = msg.getCStringAt(0);
        if (strncmp(expectedText, text, strlen(expectedText)) == 0) {
            gpBotManager->startGame();
        }
    }
}


const int BLIP_TYPE_CHAMBER = 3;
void handleBlipMsg(const NetMessage& msg)
{
    byte numMsgs = msg.getByteAt(0) & 0xF;
    int blipIdx = 0;
    int idx = 1;
    while (idx + 3 < msg.size() && blipIdx < numMsgs) {
        blipIdx++;
        Vector loc = msg.getVectorAt(idx);
        idx += 3;
        byte blipType = msg.getByteAt(idx++);
        // some blip messages only have the type and no colour byte
        if (idx < msg.size() && msg.getTypeAt(idx) == kByte) {
            /*byte blipColor =*/ msg.getByteAt(idx++);
            if (blipType == BLIP_TYPE_CHAMBER) {
                // We only get blip messages for chambers if they're under attack
                HiveMind::entityUnderAttack(loc);
            }
        }
    }
}

long damageMask = DMG_GENERIC | DMG_BULLET | DMG_SLASH | DMG_BURN | DMG_BLAST | DMG_SONIC;

void handleDamage(const NetMessage& msg)
{
	edict_t* pEdict = msg.getEdict();
	Bot* pBot = gpBotManager->getBot(pEdict);
	if (pBot != NULL) {
		if (msg.size() == 6) {
			long damageBits = msg.getLongAt(2);
			if ((damageBits & damageMask) != 0 || (damageBits == 0)) {
				// ok, odds on we actually got shot
				Vector damageOrigin(msg.getFloatAt(3), msg.getFloatAt(4), msg.getFloatAt(5));
				pBot->handleDamage(msg.getByteAt(0), msg.getByteAt(1), damageOrigin);

			} else {
				WB_LOG_DEBUG("Took damage, but don't care due to type");
			}


		} else {
			WB_LOG_ERROR("Unexpected message size for damage");
		}
	}
}

void handleGameStatusMsg(const NetMessage &msg)
{
	if (msg.size()==2 && msg.getByteAt(0) == 0)
	{
		gpBotManager->startGame();
	}
}
enum AlienInfo_ChangeFlags
{
	NO_CHANGE = 0,
	COORDS_CHANGED = 1,
	STATUS_CHANGED = 2,
	HEALTH_CHANGED = 4
};

static constexpr int DEFENSE_UPGRADE = 1;
static constexpr int MOVEMENT_UPGRADE = 3;
static constexpr int SENSORY_UPGRADE = 4;

void handleAlienInfoMsg(const NetMessage& msg)
{
	int status,upgradeType,msgIdx = 0;
	byte msgType = msg.getByteAt(msgIdx++);
	bool isHiveInfo = (msgType & 0x80) == 0;
	//WB_LOG_DEBUG("handleHiveMsg: {}", msg.toString());
	if(isHiveInfo)
	{
		Vector position;
		int numHives = msgType & 0x7F;
		AlienInfo_ChangeFlags changes;
		bool hiveUnderAttack = false;
		
		for(int hiveIdx = 0; hiveIdx < numHives; ++hiveIdx)
		{
			hiveUnderAttack = false;
			changes = static_cast<AlienInfo_ChangeFlags>(msg.getByteAt(msgIdx++));
			if((changes & COORDS_CHANGED) != 0)
			{
				if(HiveManager::getHive(hiveIdx)== NULL)
				{

					position.x = msg.getFloatAt(msgIdx++);
					position.y = msg.getFloatAt(msgIdx++);
					position.z = msg.getFloatAt(msgIdx++);
		
					WB_LOG_DEBUG("Hive {} position: {}, {}, {}", hiveIdx, position.x, position.y, position.z);
					edict_t* pHiveEntity = getHiveEntity(position);
					if(pHiveEntity != NULL)
					{
						HiveInfo info(hiveIdx, pHiveEntity);
						HiveManager::addHive(info);
						WB_LOG_DEBUG("Added hive {}", hiveIdx);
					}else
					{
						WB_LOG_ERROR("Couldn't find hive entity for hive {} at position {}, {}, {}", hiveIdx, position.x, position.y, position.z);
					}
				}else
				{
					//WB_LOG_DEBUG("Received coordinates for existing hive {}", hiveIdx);
					msgIdx += 3; // Skip over the position data since we already have an entity for this hive and don't want to mess with it.  We may want to change this later if we find that the hive entities can move around or if we want to update our stored positions based on the message data.
				}
			}
			if(changes & STATUS_CHANGED)
			{
				HiveInfo* pInfo = HiveManager::getHive(hiveIdx);
				status = msg.getByteAt(msgIdx++);
				hiveUnderAttack = (status & 0x80) != 0;
				upgradeType = (status >> 3) & 0x03;
				status &= 0x07;
				
				if (pInfo != NULL)
				{
					if (hiveUnderAttack)
						HiveMind::entityUnderAttack(pInfo->getEntity().getEdict());
					pInfo->updateHealth(status); // Note: this is not actuallly health, but the hive state (0-6) which is being stored in the health field for now.  We may want to change this later.
					WB_LOG_DEBUG("Hive {} health updated to {}", hiveIdx, status);
				}else
				{
					WB_LOG_ERROR("Received hive status update for unknown hive {}", hiveIdx);
				}
				if(upgradeType)
				{
					switch(upgradeType)
					{
					case 0:
						WB_LOG_DEBUG("No tech available from hive {}", hiveIdx);
						break;
					case 1:
						WB_LOG_DEBUG("Defense tech available from hive {}", hiveIdx);
						break;
					case 2:
						WB_LOG_DEBUG("Sensory tech available from hive {}", hiveIdx);
						break;
					case 3:
						WB_LOG_DEBUG("Movement tech available from hive {}", hiveIdx);
						break;
					default:
						WB_LOG_ERROR("Unknown tech type {} for hive {}", upgradeType, hiveIdx);	
					}
				}
			}
			if( changes & HEALTH_CHANGED )
			{
				// Note: empty hives are reported as 100% health, so we can't rely on this to detect hive deaths.
				// We don't currently use the health_changed info.
				msgIdx++; // health percentage byte
				if(isNS33Version()) msgIdx++; // build time byte in 3.3 version of message
			}
		}
	}else
	{
		int numUpgrades = msg.getByteAt(msgIdx++);
		HiveManager::resetTraits();
		for(int upgradeIdx = 0; upgradeIdx < numUpgrades; ++upgradeIdx)
		{			
			int traitId = msg.getByteAt(msgIdx++);
			HiveManager::addTraitLevel(traitId);
			switch(traitId)
			{
			case DEFENSE_UPGRADE:
				WB_LOG_DEBUG("Defense upgrades available");
				break;
			case MOVEMENT_UPGRADE:
				WB_LOG_DEBUG("Movement upgrades available");
				break;
			case SENSORY_UPGRADE:
				WB_LOG_DEBUG("Sensory upgrades available");
				break;
			default:	
				WB_LOG_ERROR("Unknown trait {} available", traitId);
			}
		}
	}
}
void NetMessageDispatcher::registerHandlers()
{
    if (_handlers.size() == 0) {
        registerHandler(handleAlienInfoMsg, "AlienInfo");
		/* Disabling damage handler because it causes crashing.*/
		registerHandler(handleDamage, "Damage");
        registerHandler(handleNetAreaInfoMsg, "SetupMap");
        //registerHandler(handleTextDisplayMsg, "HudText");
        registerHandler(handleBlipMsg, "BlipList");
		registerHandler(handleGameStatusMsg, "GameStatus");
    }
}
