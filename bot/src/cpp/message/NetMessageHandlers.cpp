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

Log netMsgLog(__FILE__);


void parseInitialHiveStatusData (const NetMessage& msg);
void parseHiveStatusUpdateData (const NetMessage& msg, int expectedHiveCount);
int getHiveIdFromStatusUpdateData (const NetMessage& msg, int expectedHiveCount);

//-------------------------------------------------------------------------
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
				netMsgLog.Debug("Added area %s", areaName);
    		}
        }
    }
}


const int DEFENSE_UPGRADE = 1;
const int MOVEMENT_UPGRADE = 3;
const int SENSORY_UPGRADE = 4;
void handleTraitsAvailableMsg(const NetMessage& msg)
{
    if (msg.size() >= 2) {
    	byte numUpgradesAvail = msg.getByteAt(1);
        HiveManager::resetTraits();

    	for (int ii = 0; ii < numUpgradesAvail; ii++) {
	    	byte traitId = msg.getByteAt(2 + ii);
		    netMsgLog.Debug("Trait %d available", traitId);

            HiveManager::addTraitLevel(traitId);
	    }
    }
}


void handleHiveStatusMsg(const NetMessage& msg)
{
	int maxHives = msg.getByteAt(0);
	if (maxHives == 1 || maxHives == 3)
	{
		if (msg.getByteAt(1) == 7)
		{
			parseInitialHiveStatusData(msg);
			netMsgLog.Debug("Handling initial hive status message");
		}
		else
		{
			parseHiveStatusUpdateData(msg,maxHives);
			netMsgLog.Debug("Handling hive status message");
		}
	}
	else
	{
		netMsgLog.ConsoleLog("WARNING: Unexpected hive count in hive status message");
	}
	
	//netMsgLog.Debug(msg.toString().c_str());
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
const int START_OF_HIVE_DATA_OFFSET = 1;
//const int HIVE_DATA_SIZE = 6;
const int HIVE_POSITION_OFFSET = 1;
const int HIVE_HEALTH_OFFSET = 4;
void parseInitialHiveStatusData (const NetMessage& msg)
{
	//  The 3.3 version at https://github.com/ENSL/NS uses the preamble byte for each hive,
	// resulting in a HIVE_DATA_SIZE of 7 instead of 6 bytes as in the 
	// last version released by unknown worlds.
	// 3.3 sets a sv_version cvar that we use to differentiate between the two. 
	static cvar_t* nsversion = CVAR_GET_POINTER("sv_nsversion");
	static const int HIVE_DATA_SIZE = nsversion ? 7:6;

	int numHives = gpBotManager->inCombatMode() ? 1 : 3;
	assert(numHives >= 1);

    if (numHives >= 1) {
        // Only do this once for all of the bots.
        if (HiveManager::getHiveCount() == 0) {
            for (int ii=0; ii < numHives; ++ii) {
                int hiveDataOffset = (ii * HIVE_DATA_SIZE) + START_OF_HIVE_DATA_OFFSET;
                int vectorOffset = hiveDataOffset + HIVE_POSITION_OFFSET;
                int healthOffset = hiveDataOffset + HIVE_HEALTH_OFFSET;
                
                Vector position = msg.getVectorAt(vectorOffset);
                byte health = msg.getByteAt(healthOffset);
				edict_t* pEntity = getHiveEntity(position);
				assert(pEntity != NULL);

                HiveInfo info(ii, pEntity);
                info.updateHealth(health);
                HiveManager::addHive(info);
                netMsgLog.Debug("parseInitialHiveStatusData:  Added hiveId=%d", ii);
				if (health > 0) {
					netMsgLog.Debug("parseInitialHiveStatusData:  Initial alien hive is %d", ii);
				}
                // TODO:  The rest of the hive data is being dropped on the floor for the time being.
            }
        }
    }
}


void parseHiveStatusUpdateData (const NetMessage& msg, [[maybe_unused]] int expectedHiveCount)
{
	int sz = msg.size();
	int hiveId = 0;
	byte msgType = 0;
	byte hiveState =0;
	HiveInfo* pInfo;

	for (int ii=1;ii<sz-1;ii++)
	{
		msgType = msg.getByteAt(ii);
		
		switch(msgType)
		{
		case 0:	//no update this hive
			hiveId++;
			break;
		case 2:	//Hive State update message
				//First 3 bits are Gestation stage
				//The others indicate type of chamber associated
				//with this hive.
				//possible gestation state values(&7):
				//0 = dead
				//1 - 5 = Gestating
				//6 = completed
				//upgrade values(&0xF8):
				//dc == 14, sc == 22, mc == 30 

			ii++;
			hiveState = msg.getByteAt(ii)&7;
			pInfo = HiveManager::getHive(hiveId);
			if (pInfo != NULL)
			{
				
				pInfo->updateHealth(hiveState);
						
				if (hiveState == 0)
				{
					
					netMsgLog.Debug("Hive %d is dead.",hiveId);
				}
				else if (hiveState < 6)
				{
					
					netMsgLog.Debug("Hive %d Gestating.",hiveId);
				}
				else
				{
					
					netMsgLog.Debug("Hive %d Complete.",hiveId);
				}
			}
			hiveId++;
			break;
		case 4:	//Hive health message 0-100 percent
				//Note: empty hives reported as 100%
			hiveId++;
			ii++;
			break;
		case 6:
			//First byte of message has high bit set if under attack
			// low 3 bits indicate hive gestate state.
			// Dont know what the other values are used for.
			//Second byte is hives current health %
			{
				pInfo = HiveManager::getHive(hiveId);
				ii++;
				if (pInfo)
				{
					hiveState = msg.getByteAt(ii);
					if ((hiveState & 0x80))
						HiveMind::entityUnderAttack(pInfo->getEntity().getEdict());
					pInfo->updateHealth((hiveState & 0x07));
				}

				ii++;
				hiveId++;
				break;
			}
		default:
			ii++;
			netMsgLog.Debug("WARNING:Unknown Hive message type %d.",msgType);
			break;
		}
	}
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
	netMsgLog.ConsoleLog ("ERROR: no hive found at given position");
	return NULL;
}


const int HIVE_INFO_MSG_TRAITS = 128;
const int HIVE_INFO_MSG_STATUS = 3;
const int CO_HIVE_INFO_MSG_STATUS = 1;

void handleHiveInfoMsg(const NetMessage& msg)
{
	byte hiveInfoType = msg.getByteAt(0);
	switch (hiveInfoType) {
	case HIVE_INFO_MSG_TRAITS:
		handleTraitsAvailableMsg(msg);
		break;

	case CO_HIVE_INFO_MSG_STATUS:
	case HIVE_INFO_MSG_STATUS:
		handleHiveStatusMsg(msg);
		break;

	default:
		netMsgLog.Debug("WARNING:Unknown hive info type message %d", hiveInfoType);
		break;
	}
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
				netMsgLog.Debug("Took damage, but don't care due to type");
			}


		} else {
			netMsgLog.Warn("Unexpected message size for damage");
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

void NetMessageDispatcher::registerHandlers()
{
    if (_handlers.size() == 0) {
        registerHandler(handleHiveInfoMsg, "AlienInfo");
		/* Disabling damage handler because it causes crashing.*/
		//registerHandler(handleDamage, "Damage");
        registerHandler(handleNetAreaInfoMsg, "SetupMap");
        //registerHandler(handleTextDisplayMsg, "HudText");
        registerHandler(handleBlipMsg, "BlipList");
		registerHandler(handleGameStatusMsg, "GameStatus");
    }
}
