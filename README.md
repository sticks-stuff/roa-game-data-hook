# roa-game-data-hook

Outputs current game data as json over a TCP socket, similar to [smush_info](https://github.com/sticks-stuff/smush_info). Useful to hook into for stream automation or set reporting.

Uses [roa-mod-loader](https://github.com/raicool/roa-mod-loader) version 0.0.3

Example output is in [example.json](example.json)

By default it advertises on port 4343 (though this can be changed in the `config.yaml`) and also advertises itself over UDP on port 6500 (can also be changed).

The advertisement looks like `roa_game_data_hook:<pc_name>:<ip_addr>`

Special shoutout to [Rai](https://github.com/raicool) for [writing the detour needed for character IDs](https://discord.com/channels/819715327893045288/819715327893045292/1468815350438039562).

# Building

```
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```