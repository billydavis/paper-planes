#pragma once

// The app: works out what the board should be doing (its Mode) once per
// loop, polls for flights, and draws. main.cpp only calls these.
//
// Serial commands (115200 baud):
//   d  dump the current screen as hex (tools/grab_screen.py turns it into a PNG)
//   f  force a full refresh
//   n  next sample flight (in live mode, shown until the next poll)

enum class Mode {
  Setup,      // running the setup hotspot
  Night,      // night hours: panel off, no lookups
  Starting,   // no data yet
  NoWifi,     // lost the network
  FeedDown,   // every ADS-B feed failed, nothing to show yet
  Live,       // showing current data
  Stale,      // showing data, but updates have stopped
  Demo,       // built-in samples
};

void appBegin();
void appTick();
