#pragma once
#include <WebServer.h>

#include <functional>

// Request checks shared by the web handlers. All of this runs on the web
// server's task.
//
//  - Host: only answer to our own name and IPs. A web page elsewhere can
//    point a hostname of its own at this board ("DNS rebinding") and then read
//    it as if it were that site; those requests carry the other site's Host.
//  - Origin: state-changing requests must come from our own pages, so another
//    site can't submit our forms behind your back (CSRF).
//  - Password: HTTP basic auth (user "admin") when a settings password is set.

void guardBegin(WebServer &server, bool setupMode);
// Current hostname and settings password (call again when they change).
void guardConfigure(const char *hostname, const char *webPass);

// Wrap a handler with the Host check. In setup mode a foreign host is a
// phone's captive-portal probe: it's sent to the settings page instead.
std::function<void()> guarded(std::function<void()> fn);

bool hostOk(const String &host);
bool sameOrigin();
bool passwordSet();
// The password, when one is set; sends the 401 challenge otherwise.
bool authorized();
// For POSTs: same origin, then the password. Sends the refusal itself.
bool authorizedPost();
// Checks without sending anything (for upload callbacks).
bool authorizedQuiet();

extern const char *WEB_USER;
