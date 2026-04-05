//
// $Id: WaypointNavMethod.h,v 1.3 2008/03/08 18:18:31 masked_carrot Exp $

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

#ifndef __WAYPOINT_NAV_METHOD_H
#define __WAYPOINT_NAV_METHOD_H

#include "BotTypedefs.h"
#include "NavigationMethod.h"
#include "BotMovement.h"
#include "framework/Log.h"
#include "extern/halflifesdk/util.h"
#include "Bot.h"

using namespace wb_pathematics;


class WaypointNavMethod : public NavigationMethod  
{
public:

	WaypointNavMethod(Bot& bot, bool initialSpawn);

	virtual ~WaypointNavMethod();

	virtual NavigationMethod* navigate();

	virtual void pause();

	virtual void resume();

	virtual std::string getName() const;

	typedef enum {
		LOST, // no target right now
		SEEKING_NEAREST,    // trying to make progress to nearest waypoint
		SEEKING_REWARDS,
		FINDING_SWITCH,
        RETRACING_STEPS,
		WAIT_AT_WAYPOINT,
		WAIT_FOR_LIFT
	} WaypointMode;

protected:

	int getNearestWaypointId();
	void giveUpOnNextWaypoint();
	void calculateNextWaypoint();
	void seekRewards();
	void seekNearestWaypoint();
	void foundRouteWaypoint();
	void findSwitch();
	Vector vectorToWaypoint(int wptId);
	void addHistoryWaypoint(int wptId);
	void resetWaypointHistory();
	void unableToFindNextWaypoint();
	bool checkStuck();
    void retraceSteps();
    static Vector getWptOrigin(tNodeId wptId);
    void navigateLost();
    void moveBetweenWaypoints();
    bool didWeHitWaypoint(Vector& vecToWpt);
    void updateWaypointTravelTime(int prevWptId, int nextWptId, float travelTime);
	void updateUnreachedWaypointTravelTime();
    void changedEvolution();
	void waitAtWaypoint();
	bool liftIsNear();
	void waitForLift();
	float getMinTargetDistance();
    bool tryToUseEntity(const char* entityClassName);
    void setNextWaypointTarget();

    bool isLadderPath(tNodeId prevWptId, tNodeId nextWptId);

	inline int prevWaypointId() { 	return (_wptHistory.size() > 0 && (_mode != RETRACING_STEPS)) ? *_wptHistory.begin() : -1; }

	Bot& _bot;
	
	std::deque<int> _wptHistory;
	
	EntityReference _lastLiftEntity;

	
	float _useButtonTime;
	float _lastWptTime;

	int _nextWptId;
	int _lastLiftCheckWpt;
	
	WaypointMode _mode;

	tEvolution _evolution;

	// If true, the bot will wait at the wp once it arrives at the wp.
	bool _waitAtWp;
private:

	static Log _log; 
};

#endif // __WAYPOINT_NAV_METHOD_H
