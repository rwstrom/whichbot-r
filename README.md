# whichbot-r
A metamod plugin for the game natural selection that provides alien bot opponents.
This is a fork of the whicbot project that can be found at https://whichbot.sourceforge.net/

This version removes the dependencies on STL_port and adds the ability to use Cmake for building.

Testing has only been done using ENSL version 3.3 of Natural Selection found [here.](https://github.com/ENSL/NS)

## Compiling
To compile with cmake under windows:
Open cmd or powershell in the whichbot-r directory and issue the following commands:
`cmake -A Win32 -B build`

`cmake --build build --config Release`

The whichbot.dll will be in the build/bot/Release folder.

To compile under linux:
Open a terminal in the whichbot-r folder.

`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`

`cmake --build build`

## Installing
Note: Whichbot-r is a metamod plugin and needs metamod to run.

I recommend using metamod-p which can be found [here](https://metamod-p.sourceforge.net/) or [here.](https://github.com/Bots-United/metamod-p)

After installing metamod you will need to create or edit the metamod plugins.ini file found in {Halflife intstall directory}/addons/metamod .

Add the lines:  
```win32 addons/whichbot/dlls/whichbot.dll```

```linux addons/whichbot/dlls/libwhichbot.so```

### Installing from precompiled binary:

Download the zip or gz file from the release page and extract the files to your `{hlds install directory}/ns` directory.

### Install from source files:

Compile as instructed above.

Create the folder `{hlds install directory}/ns/addons/whichbot/data`

Create the folder `{hlds install directory}/ns/addons/whichbot/dlls`

Copy the whichbot dll/so that you built into `{hlds install directory}/ns/addons/whichbot/dlls`

Copy the folder `bot/conf` into  `{hlds install directory}/ns/addons/whichbot`

Copy the files from `bot/data/waypoints` into ` {hlds install directory}/ns/addons/whichbot/data`

## Basic usage:

**To add a bot:**

`wb add`

**To remove a bot:**

`wb remove`

The configuration file can be found in addons/whichbot/conf/whichbot.txt