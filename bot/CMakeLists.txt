add_library(whichbot SHARED)
set_property(TARGET whichbot PROPERTY CXX_STANDARD 23)
if(MSVC)
    target_compile_options(whichbot PRIVATE /W4)
else()
    target_compile_options(whichbot PRIVATE -m32 -fPIC)
    target_link_options(whichbot PRIVATE -m32)
endif()

target_include_directories(whichbot PRIVATE src/cpp)
target_include_directories(whichbot PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../pathematics/src/cpp")

target_sources(whichbot PRIVATE src/cpp/Bot.cpp
                                src/cpp/BotManager.cpp
                                src/cpp/StatusReporter.cpp)   

target_sources(whichbot PRIVATE src/cpp/combat/CombatNavMethod.cpp
                                src/cpp/combat/CombatStrategy.cpp
                                )

target_sources(whichbot PRIVATE src/cpp/config/Config.cpp
                                src/cpp/config/EntityInfo.cpp
                                src/cpp/config/TranslationManager.cpp
                                )

target_sources(whichbot PRIVATE src/cpp/extern/metamod/h_export.cpp
                                src/cpp/extern/halflifesdk/util.cpp)

target_sources(whichbot PRIVATE src/cpp/framework/Log.cpp)

target_sources(whichbot PRIVATE src/cpp/linkage/dllapi.cpp
                                src/cpp/linkage/engine_api.cpp
                                src/cpp/linkage/engine_intercepts.cpp
                                src/cpp/linkage/meta_api.cpp
                                src/cpp/linkage/plugin.cpp)

target_sources(whichbot PRIVATE src/cpp/message/ClientCommandDispatcher.cpp
                                src/cpp/message/ClientCommandHandlers.cpp
                                src/cpp/message/NetMessage.cpp
                                src/cpp/message/NetMessageDispatcher.cpp
                                src/cpp/message/NetMessageElement.cpp
                                src/cpp/message/NetMessageHandlers.cpp)   

target_sources(whichbot PRIVATE src/cpp/navigation/BotMovement.cpp
                                src/cpp/navigation/BuildNavMethod.cpp
                                src/cpp/navigation/NavigationEngine.cpp
                                src/cpp/navigation/NavigationMethod.cpp
                                src/cpp/navigation/WaypointDebugger.cpp
                                src/cpp/navigation/WaypointManager.cpp
                                src/cpp/navigation/WaypointNavMethod.cpp)       

target_sources(whichbot PRIVATE src/cpp/sensory/AuditoryManager.cpp
                                src/cpp/sensory/AuditoryTrace.cpp
                                src/cpp/sensory/BotSensor.cpp
                                src/cpp/sensory/Target.cpp)                                

target_sources(whichbot PRIVATE src/cpp/strategy/AmbushStrategy.cpp
                                src/cpp/strategy/AttackStrategy.cpp
                                src/cpp/strategy/BuilderStrategy.cpp
                                src/cpp/strategy/FleeStrategy.cpp
                                src/cpp/strategy/HiveMind.cpp
                                src/cpp/strategy/PackFollowerStrategy.cpp
                                src/cpp/strategy/PackInfo.cpp
                                src/cpp/strategy/PackLeaderStrategy.cpp
                                src/cpp/strategy/PackManager.cpp
                                src/cpp/strategy/RescueStrategy.cpp
                                src/cpp/strategy/ScoutStrategy.cpp
                                src/cpp/strategy/StatusReportElement.cpp
                                src/cpp/strategy/Strategy.cpp
                                src/cpp/strategy/StrategyManager.cpp)

target_sources(whichbot PRIVATE src/cpp/worldstate/AreaInfo.cpp
                                src/cpp/worldstate/AreaManager.cpp
                                src/cpp/worldstate/EntityReference.cpp
                                src/cpp/worldstate/EntityRegistry.cpp
                                src/cpp/worldstate/HiveInfo.cpp
                                src/cpp/worldstate/HiveManager.cpp
                                src/cpp/worldstate/WorldStateUtil.cpp)

if(MSVC)
    target_sources(whichbot PRIVATE src/cpp/linkage/bot.def)
    target_link_libraries(whichbot PRIVATE pathematics)
else()

target_link_libraries(whichbot PRIVATE pathematics stdc++)
endif()

if(MAKE_ZIP)
add_custom_command(
  TARGET whichbot POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/addons/whichbot/dlls"
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:whichbot> "${CMAKE_BINARY_DIR}/addons/whichbot/dlls"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/conf"  "${CMAKE_BINARY_DIR}/addons/whichbot/conf"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/data/waypoints" "${CMAKE_BINARY_DIR}/addons/whichbot/data"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/addons/whichbot/docs"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/docs/WhichbotWaypointingGuide.doc" "${CMAKE_BINARY_DIR}/addons/whichbot/docs/"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/docs/WhichbotWaypointingGuide.pdf" "${CMAKE_BINARY_DIR}/addons/whichbot/docs/"
  COMMAND ${CMAKE_COMMAND} -E tar "czf" "whichbot-r.tar.gz" "${CMAKE_BINARY_DIR}/addons"
  VERBATIM)
  endif()