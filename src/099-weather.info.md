# 099-weather

*Written by `scripts/generate-info` from `099-weather.h`; don't edit, re-run it.*

099-weather.h (issue 1109 step 1v) - the server's weather, as a storm's
strength for the light

For a general audience: the server tells the client what the weather
is where the player stands (rain, snow, a sandstorm, fine) and how
heavy it is. The client lights a stormy place with each light's storm
colours and fog (026's light_at_weather), more of them the heavier the
weather, and, unless the server says the change is sudden, eases from
the old weather to the new over a few seconds. This reads that message
and keeps the eased strength.

The message, SMSG_WEATHER (3.3.5a, build 12340), its body after the
header, 9 bytes, little-endian:
  0  uint32  the weather's state (WEATHER_* below)
  4  float   its grade, how heavy: 0 to 1
  8  uint8   abrupt: 1 when the change is at once, 0 to ease into it
How sure each part is (1109 decision 157): all of it is recalled, not
read from code. This client has no network code yet (Phase 5), its docs
(004) don't list the message, and no server source is on this machine;
the opcode, the field order, the 9-byte length, that 3.3.5a sends no
sound id (older clients' messages had one after the state) and the
state numbers are unchecked. A body of another length is refused,
naming both, so a wrong guess shows itself on the first real message.

Window-free (CORE: the test program and the executable).

## What it offers

- `type WeatherMsg`
  One message's contents.
- `WErr weather_msg_read(const unsigned char *body, size_t len, WeatherMsg *out, WWhy *why);`
  Reads a message's body (`len` bytes after its header). W_OK with
  `*out`; W_ERR_CORRUPT for a body that isn't WEATHER_BODY_BYTES long, or
  whose grade isn't a number, naming what it found.
- `float weather_msg_storm(const WeatherMsg *m);`
  The storm's strength a message asks for: 0 for fine weather, else its
  grade (decision 158).
- `type Weather`
  The weather as the light sees it over time (the handle's state is
  plain data: no allocation). `seconds` is any steady clock: the
  window's time, or the run with no window's steps.
- `void weather_init(Weather *w);`
  Fine weather, no storm, nothing taken.
- `void weather_take(Weather *w, const WeatherMsg *m, double seconds);`
  Takes a message at `seconds`: the strength eases from where it stands
  then to the message's over WEATHER_FADE_SECONDS, or is there at once
  when the message is abrupt.
- `float weather_strength(const Weather *w, double seconds);`
  The storm's strength at `seconds` (0-1), for light_at_weather.
