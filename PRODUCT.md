# Product

## Register

product

## Users
One household: the person who built the board and lives near a small teaching airport. They open the board's web page (http://flights.local) mostly on a phone, briefly, when a plane is overhead or the e-paper panel is stuck on a training aircraft. Typical job: glance at what is on the panel, hide a trainer for a few hours, maybe change a setting, close the page. Occasionally from a desktop browser for settings and firmware updates.

## Product Purpose
paper-planes shows the nearest aircraft on a 5.79" e-paper panel. The web page is its companion: a live copy of the panel, the flight details, the controls that steer what the panel shows (mute a plane, settings), and device status. It is served by the ESP32 itself, often with no internet, so it must be small and self-contained. Success: the person can see and fix what the panel shows in one or two taps, without reading.

## Brand Personality
Calm, precise, instrument-like. It should feel like a piece of aviation kit (a flight strip, a cockpit annunciator, the e-paper card itself) rather than an app. Quiet until something needs attention; utility first, no decoration that doesn't carry information.

## Anti-references
- Generic SaaS dashboards: rounded card grids, gradients, big KPI numbers.
- Flight-tracker clutter: dense FlightRadar24-style overload, busy maps, ad-like noise.
- Neon or gamer dark mode: glowing accents, heavy dark-tech styling.

## Design Principles
1. The panel is the hero. The web page mirrors and serves the e-paper card; it never competes with it.
2. One glance, one tap. Primary actions (hide this plane) sit next to what they act on and finish in a single tap, with undo instead of confirmation.
3. Quiet by default, loud only for state. Color and emphasis are reserved for status (live, failing, emergency, hidden).
4. Speak plainly. Labels say what happens ("Hide N776CA for 3 h"), not system terms.
5. Small and offline. No external fonts, libraries or images; everything ships gzipped in the firmware.

## Accessibility & Inclusion
WCAG 2.2 AA: text and controls meet contrast in light and dark themes, touch targets at least 44px on phone, full keyboard use with visible focus, status changes announced to screen readers, and motion respects prefers-reduced-motion.
