# whichbot-r
A metamod plugin for the game natural selection that provides alien bot opponents.
This is a fork of the whicbot project that can be found at https://whichbot.sourceforge.net/

This version removes the dependencies on STL_port, upgrades the getpot library and adds CMakeLists.txt for compiling.

To compile with cmake under windows:
Open cmd or powershell in the whichbot-r directory and issue the following commands:
`cmake -A Win32 -B build`

`cmake --build build --config Release`

The whichbot.dll will be in the build/bot/Release folder.

To compile under linux:
Open a terminal in the whichbot-r folder.

`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`

`cmake --build build`
