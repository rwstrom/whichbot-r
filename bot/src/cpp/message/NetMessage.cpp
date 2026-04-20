//
// $Id: NetMessage.cpp,v 1.5 2007/09/02 00:41:32 masked_carrot Exp $

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

#include "NetMessage.h"
#include <sstream>
#include "../extern/metamod/meta_api.h"
Log NetMessage::_log(__FILE__);


NetMessage::NetMessage(int msgType, edict_t* pEdict) :
    _pEdict(pEdict),
    _msgType(msgType)
{
}


NetMessage::~NetMessage()
{
    reset(0, NULL);
}


void NetMessage::reset(int msgType, edict_t* pEdict)
{
    _msgType = msgType;
    _pEdict = pEdict;
    
    for (std::vector<NetMessageElement*>::iterator ii = _elements.begin(); ii != _elements.end(); ii++) {
        delete *ii;
    }
    _elements.clear();
}


void NetMessage::addElement(void* rock, eNetMessageType type)
{
	if (type < 0 || type > kCString){
		_log.Warn("Invalid type in NetMEssage::addElement %d",type);;
		return;
	}//tmc
    NetMessageElement* element = new NetMessageElement(rock, type);
    _elements.push_back(element);
}


int NetMessage::size() const
{
    return _elements.size();
}


bool NetMessage::isEmpty() const
{
	return ((_msgType == 0) && (size() == 0));
}


const NetMessageElement* NetMessage::getElementAt(int ii) const
{
    if (ii < 0 || ii >= size()) {
        return NULL;
        
    } else {
        return _elements[ii];
    }
}


byte NetMessage::getByteAt(int ii) const
{
    assert(ii >= 0 && ii < size());
    assert(_elements[ii]->type == kByte);
    if (ii < 0 || ii >= size())
	{
		_log.Warn("Invalid index for NetMessage::getByteAt");
		return 0;
	}
	if (_elements[ii]->type != kByte)
	{
		_log.Warn("Wrong type requested in NetMessage::getByteAt");
		return 0;
	}
    return *(byte*)_elements[ii]->rock;
}


int NetMessage::getIntAt(int ii) const
{
    assert(ii >= 0 && ii < size());
    assert(_elements[ii]->type == kInt);
    if (ii < 0 || ii >= size())
	{
		_log.Warn("Invalid index requested in NetMessage::getIntAt");
		return 0;
	}
	if (_elements[ii]->type != kInt)
	{
		_log.Warn("Wrong type requested in NetMessage::getIntAt");
		return 0;
	}
    return *(int*)_elements[ii]->rock;
}


long NetMessage::getLongAt(int ii) const
{
    assert(ii >= 0 && ii < size());
    assert(_elements[ii]->type == kLong);
    if (ii < 0 || ii >= size())
	{
		_log.Warn("Invalid index for NetMessage::getLongAt");
		return 0;
	}
	if (_elements[ii]->type != kLong)
	{
		_log.Warn("Wrong type requested in NetMessage::getLongAt");
		return 0;
	}
    return *(long*)_elements[ii]->rock;
}


short NetMessage::getShortAt(int ii) const
{
    assert(ii >= 0 && ii < size());
    assert(_elements[ii]->type == kShort);
    if (ii < 0 || ii >= size())
	{
		_log.Warn("Invalid index for NetMessage::getShortAt");
		return 0;
	}
	if (_elements[ii]->type != kShort)
	{
		_log.Warn("Wrong type requested in NetMessage::getShortAt");
		return 0;
	}
    return *(short*)_elements[ii]->rock;
}


float NetMessage::getFloatAt(int ii) const
{
    assert(ii >= 0 && ii < size());
    assert(_elements[ii]->type == kFloat);
    if (ii < 0 || ii >= size())
	{
		_log.Warn("Invalid index for NetMessage::getFloatAt");
		return 0;
	}
	if (_elements[ii]->type != kFloat)
	{
		_log.Warn("Wrong type requested in NetMessage::getFloatAt");
		return 0;
	}
    return *(short*)_elements[ii]->rock;
}


Vector NetMessage::getVectorAt(int ii) const
{
    assert(ii >= 0 && ii+2 < size());
    assert(_elements[ii]->type == kFloat);
    assert(_elements[ii+1]->type == kFloat);
    assert(_elements[ii+2]->type == kFloat);
    if (ii < 0 || ii+2 >= size())
	{
		_log.Warn("Invalid index for NetMessage::getVectorAt");
		return Vector(0,0,0);
	}
	if (_elements[ii]->type != kFloat || _elements[ii+1]->type != kFloat ||_elements[ii+2]->type != kFloat)
	{
		_log.Warn("Wrong type requested in NetMessage::getVectorAt");
		return Vector(0,0,0);
	}
    float xPos = *(float*)_elements[ii]->rock;
    float yPos = *(float*)_elements[ii+1]->rock;
    float zPos = *(float*)_elements[ii+2]->rock;
    return Vector(xPos, yPos, zPos);
}


const char* NetMessage::getCStringAt(int ii) const
{
    assert(ii >= 0 && ii < size());
    assert(_elements[ii]->type == kCString);
    if (ii < 0 || ii >= size())
	{
		_log.Warn("Invalid index in NetMessage::getCString");
		return NULL;
	}
	if (_elements[ii]->type != kCString)
	{
		_log.Warn("Wrong type requested in NetMessage::getCStringAt");
		return NULL;
	}
    return (const char*)_elements[ii]->rock;
}


std::string NetMessage::toString() const
{
    std::ostringstream oss;
    for (int ii = 0; ii < size(); ++ii) {
        oss << "[" << ii << "]" << getElementAt(ii)->toString().c_str();
    }
    return oss.str();
}


eNetMessageType NetMessage::getTypeAt(int ii) const
{
    if (ii >= 0 && ii < size()) {
        return _elements[ii]->type;
    }
	_log.Warn("Unknown type in getTypeAt");
    return kUnknownType;
}


edict_t* NetMessage::getEdict() const
{
    return _pEdict;
}


void NetMessage::log()
{
	const char* className = FNullEnt(_pEdict) ? "No Entity" : STRING(_pEdict->v.classname);
	int sz;
	const char* msgName = GET_USER_MSG_NAME(PLID,_msgType,&sz);//tmc
	if (msgName != NULL) _log.FileLog(msgName);
	_log.FileLog(className);
	_log.Debug("Starting message %s for edict 0x%x (%s)", msgName, _pEdict, className);
	for (std::vector<NetMessageElement*>::iterator ii = _elements.begin(); ii != _elements.end(); ii++) {
		_log.FileLog((*ii)->toString().c_str());
	}
	_log.Debug("Ending message %s for edict 0x%x (%s)", msgName, _pEdict, className);
}
