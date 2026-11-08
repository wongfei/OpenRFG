# OpenRFG

## Quick start

- [Original bugs](original_bugs.md)
- [Port deviations](deviations.md)

```
git clone https://github.com/wongfei/OpenRFG.git
cd OpenRFG

cmake -S "." -B "build\x86" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENRFG_GAME_DIR="C:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered"

cmake --build "build\x86"

set SHIM_MCCOMPAT=0x800000001
"build\x86\src\openrfg\OpenRFG.exe" -gamebuild "C:\Program Files (x86)\Steam\steamapps\common\Red Faction Guerrilla Re-MARS-tered\data"
```

## Debug command line arguments

```
--game-trace trace.log
--notitle # skip intro and title
--sp # start new game
--sp-load N # load game from slot N
--no-steam # disable steam
--host # host LAN game
--players N # wait for N players to join
--join # join LAN game
--ip 127.0.0.1
--port 1005
--name Player
--window-pos 0 0
```

## Multiplayer debug scenario

```
set SHIM_MCCOMPAT=0x800000001

start "OpenRFG server" "build\x86\src\openrfg\OpenRFG.exe" -gamebuild "C:\Program Files (x86)\Steam\steamapps\common\Red Faction Guerrilla Re-MARS-tered\data" --no-steam --name Host --host --players 2 --window-pos 0 0

ping -n 6 127.0.0.1 >nul

start "OpenRFG client" "build\x86\src\openrfg\OpenRFG.exe" -gamebuild "C:\Program Files (x86)\Steam\steamapps\common\Red Faction Guerrilla Re-MARS-tered\data" --no-steam --name Client --join --window-pos 1200 0
```
