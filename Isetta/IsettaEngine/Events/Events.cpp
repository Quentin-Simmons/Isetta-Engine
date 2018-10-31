/*
 * Copyright (c) 2018 Isetta
 */
#include <Events/Events.h>
#include <execution>
#include "Core/Debug/Logger.h"
#include "Core/Time/Time.h"

using namespace Isetta;
U16 Events::totalListeners = 0;

void Events::RaiseEventQueue(const EventObject& eventObject) {
  eventQueue.push(eventObject);
}

void Events::RaiseEventImmediate(const EventObject& eventObject) {
  StringId eventNameId{SID(eventObject.eventName.c_str())};
  if (callbackMap.count(eventNameId) > 0) {
    // prevent unregistering the callback from screwing up the range-based for
    // loop
    std::vector<std::pair<U16, Action<EventObject>>> callbacks{
        callbackMap.at(eventNameId)};
    for (const auto& callbackPair : callbacks) {
      callbackPair.second(eventObject);
    }
  } else {
    LOG_WARNING(Debug::Channel::Gameplay, Debug::Verbosity::Warning,
                "Event %s has no listener.", eventObject.eventName);
  }
}

U16 Events::RegisterEventListener(std::string_view eventName,
                                  const Action<EventObject>& callback) {
  StringId eventNameId{SID(eventName.data())};
  U16 handle = totalListeners++;
  if (callbackMap.count(eventNameId) > 0) {
    callbackMap.at(eventNameId).emplace_back(std::make_pair(handle, callback));
  } else {
    callbackMap.insert(std::make_pair(
        eventNameId,
        std::vector<CallbackPair>{std::make_pair(handle, callback)}));
  }
  return handle;
}

void Events::UnregisterEventListener(std::string_view eventName,
                                     U16 eventListenerHandle) {
  StringId eventNameId{SID(eventName.data())};
  if (callbackMap.count(eventNameId) > 0) {
    std::vector<CallbackPair>& callbacks = callbackMap.at(eventNameId);
    callbacks.erase(
        std::remove_if(std::begin(callbacks), std::end(callbacks),
                       [eventListenerHandle](const CallbackPair& pair) {
                         return pair.first == eventListenerHandle;
                       }),
        std::end(callbacks));
  } else {
    LOG_WARNING(Debug::Channel::Gameplay, Debug::Verbosity::Warning,
                "Event %s has no listener.", eventName);
  }
}

void Events::Update() {
  while (!eventQueue.empty()) {
    EventObject currEvent = eventQueue.top();
    if (currEvent.timeFrame > Time::GetTimeFrame()) {
      break;
    }
    eventQueue.pop();
    RaiseEventImmediate(currEvent);
  }
}