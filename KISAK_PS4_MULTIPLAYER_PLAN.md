# Kisak-Strike PS4 Multiplayer Plan

Status: design proposal  
Target: OpenOrbis PS4 homebrew, no PSN or Steam runtime dependency  
Priority: offline stability first, LAN second, Internet dedicated servers third,
player-hosted NAT traversal last

## Recommendation

Steam P2P will not be used on PS4. The replacement is an open, host-centric
session stack consisting of:

- ICE candidate negotiation;
- Cloudflare STUN for public-address discovery;
- Cloudflare TURN for relay fallback;
- a small HTTPS/WebSocket service for signaling, lobbies, invites, and
  short-lived TURN credential issuance;
- Source UDP and `CNetChan` for actual match traffic after ICE selects a route.

The best practical PS4 multiplayer path is:

1. Preserve Source's existing UDP protocol and `CNetChan` implementation.
2. Bring up LAN listen-server play over OpenOrbis BSD sockets.
3. Support Internet community/dedicated servers with public UDP endpoints.
4. Add a small HTTPS server directory and session-signaling service.
5. Replace Steam P2P with ICE for player-hosted listen servers, using direct UDP
   whenever possible and Cloudflare Realtime STUN/TURN as the fallback relay.

Cloudflare Realtime TURN is technically usable, but it is not a replacement for
a Source server, master-server directory, lobby service, or authentication
service. It only helps two UDP endpoints discover or relay a path through NATs
and firewalls. Integrating it requires an ICE/STUN/TURN client in the PS4 build
and a trusted backend that issues short-lived TURN credentials.

This is not a proposal to retain Steam behind a compatibility flag. All runtime
calls to `ISteamNetworking`, Steam lobbies, Steam matchmaking, Steam identity,
and Steam relay services must be absent or compiled out of the PS4 build.

Do not route normal dedicated-server traffic through TURN. A publicly reachable
community server is simpler, cheaper, lower latency, and compatible with the
existing Source connection flow.

## Existing multiplayer architecture

The port should reuse these layers rather than invent a new game protocol:

- `engine/net_ws.cpp`
  owns UDP/TCP socket creation, bind, polling, packet reception,
  `NET_SendPacket`, `NET_OpenSockets`, and public-address handling.
- `engine/net_chan.cpp`
  implements Source sequencing, reliable/unreliable streams, fragmentation,
  compression, flow statistics, timeouts, and packet transmission. This layer
  should remain unchanged.
- `engine/net_support.cpp`
  exposes local/public server addresses to matchmaking and bridges session
  operations to the engine network layer.
- `engine/cl_main.cpp` and `engine/sv_main.cpp`
  own client connection and server/listen-server behavior.
- `matchmaking/mm_session_offline_custom.cpp`
  is the closest starting point for local and invite-based sessions without
  Steam services.
- `matchmaking/mm_session_online_host.cpp`,
  `matchmaking/mm_session_online_client.cpp`, and
  `matchmaking/mm_session_online_search.cpp`
  contain the lifecycle that can later be adapted to a small open matchmaking
  service.
- `matchmaking/sys_session.cpp`
  directly uses `ISteamNetworking` P2P packets and Steam lobby chat for session
  control and voice-related traffic. Those calls need a non-Steam replacement;
  they are separate from ordinary Source gameplay packets.
- `matchmaking/steam_lobbyapi.cpp` and Steam search/datacenter code
  should not be enabled on PS4. Replace their discovery/signaling role, not
  their API surface wholesale.

The important separation is that gameplay already has a mature UDP transport.
Steam P2P is mainly a lobby/session side channel. ICE/TURN should sit below the
gameplay datagram API, while a small HTTPS/WebSocket service replaces lobby
discovery and candidate exchange.

## Steam P2P replacement contract

The PS4 implementation must replace each Steam responsibility explicitly:

| Steam responsibility | PS4 replacement |
|---|---|
| Steam ID / lobby member identity | Random 128-bit `KisakPeerId` plus a backend-signed session token |
| Lobby creation and discovery | Kisak HTTPS/WebSocket session service |
| Lobby chat control messages | Reliable signaling messages over WebSocket |
| P2P candidate discovery | ICE using local and Cloudflare STUN candidates |
| Steam relay fallback | Cloudflare Realtime TURN |
| P2P session accept callbacks | Explicit invite/join authorization in the session state machine |
| P2P packet availability/read | ICE transport receive queue |
| Unreliable P2P send | ICE-selected UDP path |
| Reliable P2P send | WebSocket control plane initially; a small sequenced/acknowledged channel only if runtime traffic requires it |
| Steam server browser | Kisak server directory |
| Steam authentication / VAC | Out of scope; use session tokens, server challenges, bans, and protocol checks |

Use a host-centric topology for listen-server matches. Every remote console
establishes one ICE connection to the listen-server host; clients do not form a
full peer mesh. This matches Source's client/server architecture, reduces TURN
allocations, and avoids exposing gameplay state to a new P2P abstraction.

The replacement should expose a new interface such as `IKisakSessionTransport`,
not implement a fake `ISteamNetworking`. A fake Steam API would preserve Steam
IDs, callback semantics, lobby assumptions, and channel behavior that are no
longer valid. Adapt `sys_session.cpp` at its session-message boundary instead.

Suggested session transport operations:

```text
CreateSession / JoinSession / LeaveSession
GetLocalPeerId / EnumeratePeers
PublishCandidate / OnRemoteCandidate
AuthorizePeer / RejectPeer
SendReliableControl
SendUnreliableSessionMessage
GetSelectedRoute / GetRelayStatus
```

Once a peer is authorized and ICE selects a route, bind that route to the
existing logical Source server/client address and let `CNetChan` carry gameplay
reliability, fragmentation, ordering, and voice payloads.

## Steam access assessment

Native Steam access is not available to the OpenOrbis PS4 port. A normal Steam
game links Valve's proprietary Steamworks runtime and calls `SteamAPI_Init()` to
acquire interfaces from a running Steam client. Valve documents that successful
initialization requires a running Steam client, a recognized App ID, the same OS
user context, a configured Steamworks application, and an active account that
owns the application.

The Kisak repository contains Steamworks headers and code written against the
Steam ABI, but those headers are not an implementation. The current source
expects external proprietary functionality including:

- `steam_api` linkage from launcher and matchmaking project definitions;
- `ISteamUser` and a logged-in Steam ID;
- `ISteamMatchmaking` lobbies and callbacks;
- `ISteamNetworking` P2P packet queues;
- Steam authentication and ownership tickets;
- Steam relay and backend services.

There is no official Steam client or Steamworks runtime for an OpenOrbis PS4
homebrew executable. Consequently, this port cannot call `SteamAPI_Init()` and
obtain usable Steam interfaces, generate native Steam session tickets, access
Steam lobbies, or use Steam P2P/SDR. A `steam_appid.txt` file does not replace
the client, license, runtime, or publisher configuration.

The port also does not control Valve's CS:GO Steamworks application or its
publisher credentials. Steam authentication tickets are issued by a logged-in
Steam client and validated through Steam interfaces or publisher-authorized
backend APIs. They cannot be recreated locally or replaced with an arbitrary
Steam Web API key.

Official references:

- [SteamAPI initialization requirements](https://partner.steamgames.com/doc/api/steam_api)
- [Steam user authentication and ownership](https://partner.steamgames.com/doc/features/auth)
- [Steam multiplayer feature separation](https://partner.steamgames.com/doc/features/multiplayer)

### Browser Steam login is not native Steam access

Steam supports browser-based OpenID, so a separate website could optionally let
a user prove control of a Steam account and link its Steam ID to a Kisak
profile. That browser flow would not give the PS4 executable Steamworks
interfaces, Steam P2P, SDR, lobbies, VAC, inventory, or native ownership
tickets. Ownership checks also require appropriate publisher access.

Browser Steam linking may be offered later as an optional profile feature. It
must not be required for launching the port, joining a LAN game, browsing Kisak
servers, or using Cloudflare STUN/TURN.

### PS4 build policy

- Define `NO_STEAM` for every PS4 target.
- Do not package or load `steam_api`, Steam client libraries, or reconstructed
  proprietary Steam components.
- Compile out calls to `ISteamUser`, `ISteamMatchmaking`, `ISteamNetworking`,
  Steam Game Coordinator, SDR, Steam Voice, Workshop, inventory, achievements,
  and VAC.
- Keep Steam-specific code available only for supported PC builds.
- Replace each required runtime responsibility at a narrow interface boundary;
  do not create a fake global Steam API context.

## Accountless PS4 identity model

The first PS4 multiplayer release should not require any online account. On
first launch, generate a persistent random device key under
`/data/kisak-strike/identity/`. Derive an opaque public `KisakPeerId` from that
key and never treat a display name as identity.

For online sessions:

1. The client proves possession of its device key to the Kisak session backend.
2. The backend returns a short-lived signed session token scoped to one lobby or
   server connection.
3. Invite codes or server-directory entries identify the destination.
4. The signaling service exchanges ICE candidates and obtains short-lived TURN
   credentials without exposing Cloudflare account secrets.
5. The listen or dedicated server verifies the session token and completes the
   normal Source challenge/reservation flow before allocating a player slot.

This identity is sufficient for session membership, reconnects, basic bans, and
rate limits, but it is not strong ownership proof because a homebrew user can
delete local data and regenerate a device identity. Public matchmaking should
therefore combine device keys with server-side rate limits, IP/network abuse
signals, invite controls, and moderator bans. A dedicated non-Steam account
system can be added later if stronger persistence is required.

Display names are user-selected, non-unique, and untrusted. Sanitize their
length and character set, and attach moderation actions to `KisakPeerId` or a
future Kisak account ID rather than the visible name.

## Delivery modes

### 1. Offline listen server

This remains the first acceptance target. It proves server/client simulation,
bot play, map transitions, and netchannel loopback without external services.

### 2. LAN multiplayer

Use native OpenOrbis UDP sockets and the existing Source server port. Initially
support direct `connect <address>:<port>` and a manually entered address.
Broadcast discovery can follow if OpenOrbis broadcast behavior is reliable.

LAN requires no STUN, TURN, Cloudflare account, or backend.

### 3. Internet dedicated/community servers

This is the recommended first Internet mode. Run a compatible Kisak dedicated
server on a public Linux host or a port-forwarded community machine. The PS4
client connects with ordinary Source UDP.

A minimal directory service needs only:

- server registration with short heartbeat expiry;
- address, map, build/content protocol, player counts, and region;
- signed challenge or server ownership token;
- filtered server-list queries;
- optional health probe from the directory.

The directory must not proxy gameplay. It returns an endpoint and the client
then uses the existing Source connection protocol.

### 4. Internet player-hosted listen servers

Add this only after LAN and dedicated servers are stable. Use ICE candidate
gathering and connectivity checks:

1. Gather the bound local UDP address.
2. Query Cloudflare STUN for the server-reflexive public address.
3. Obtain a Cloudflare TURN allocation and relay candidate when credentials are
   available.
4. Exchange candidates through the signaling service.
5. Test candidate pairs and select the lowest-priority working route in this
   order: LAN/local, direct public UDP, NAT-punched UDP, TURN/UDP relay, then
   TURN/TCP or TURN/TLS only as a last-resort compatibility mode.
6. Keep the selected route alive and detect consent/path loss.

Cloudflare's port 53 alternative should not be tried on the critical path unless
testing proves it useful. The credential response includes it, but timeouts can
delay non-trickle candidate gathering.

## Cloudflare feasibility

Cloudflare Realtime currently provides:

- free, unlimited STUN at `stun.cloudflare.com`;
- authenticated TURN over UDP on ports 3478 and 53;
- TURN over TCP on ports 3478 and 80;
- TURN over TLS on ports 5349 and 443;
- short-lived credential generation through its REST API;
- usage analytics by TURN key, username, and custom identifier;
- a global anycast-style service footprint outside Cloudflare's China network.

The service can relay arbitrary UDP application data through standard TURN; it
does not require Source packets to become WebRTC data channels. A native TURN
or ICE client is still required because TURN is a protocol with allocations,
permissions, authentication, refreshes, and channel bindings—not a transparent
UDP proxy.

Recommended client dependency: evaluate a small, vendored build of
[libjuice](https://github.com/paullouisageneau/libjuice), an MPL-2.0 C ICE
library supporting STUN, TURN, and UDP data. It is a better fit than importing
full WebRTC. Before adoption, prove that its socket, DNS, threading, timing, and
randomness hooks cross-compile cleanly with OpenOrbis. If that proof fails,
implement only the required RFC 8489/8656 subset behind the same transport
interface; do not mix TURN parsing into `CNetChan`.

Cloudflare-specific constraints:

- Never put a TURN key or Cloudflare API token in `eboot.bin`, configuration,
  or packaged assets.
- A trusted backend must call Cloudflare's credential-generation API and return
  only short-lived username/password credentials to the console.
- Set credential TTL slightly longer than the maximum expected match and
  support refresh for long sessions.
- Tag credentials with an opaque session/user identifier for abuse analysis.
- Revoke credentials when a session is terminated for abuse where practical.
- Resolve Cloudflare hostnames normally. Do not hardcode relay IP addresses.
- TURN credentials authorize relay usage; they do not authenticate a player or
  prove permission to join a Kisak server.

Cloudflare documents a current free allowance and usage-based TURN pricing, but
pricing is operational policy and must be rechecked before public release.

Official references:

- [Generate TURN credentials](https://developers.cloudflare.com/realtime/turn/generate-credentials/)
- [Cloudflare Realtime TURN FAQ](https://developers.cloudflare.com/realtime/turn/faq/)
- [TURN analytics](https://developers.cloudflare.com/realtime/turn/analytics/)
- [Replacing existing TURN servers](https://developers.cloudflare.com/realtime/turn/replacing-existing/)
- [Custom TURN domains](https://developers.cloudflare.com/realtime/turn/custom-domains/)

## Proposed client architecture

Introduce a narrow datagram transport beneath Source networking:

```text
CNetChan / Source connection protocol
                 |
        NET_SendPacket / receive pump
                 |
        IPs4DatagramTransport
          |                 |
  NativeUdpTransport   IceUdpTransport
                            |
                 direct candidate or TURN
```

Suggested interface responsibilities:

- open/close and bind a gameplay socket;
- send a datagram to a logical session peer;
- poll received datagrams and report the logical peer;
- expose local, reflexive, and relay candidates;
- report route changes, RTT, and relay usage;
- refresh TURN allocations and ICE consent;
- fall back or disconnect deterministically when the path expires.

`NativeUdpTransport` should be the default for loopback, LAN, and dedicated
servers. `IceUdpTransport` should be created only for a signaled player-hosted
session. Preserve the original Source packet bytes, MTU expectations, packet
ordering, and `CNetChan` timing.

Do not overload a TURN relay address as the server's canonical `netadr_t`.
Maintain a session-route table that maps a logical peer/server identifier to the
selected ICE path. This avoids leaking candidate changes into netchannel
identity, reservation, ban, and reconnect logic.

## Signaling and service design

The service plane may run on any HTTPS/WebSocket backend. Cloudflare Workers and
Durable Objects are an optional deployment choice, but Cloudflare TURN does not
provide signaling automatically.

Minimum APIs:

- `POST /v1/sessions` — create a host session and opaque join code;
- `POST /v1/sessions/{id}/candidates` — submit candidates incrementally;
- `GET` or WebSocket stream for remote candidates and session state;
- `POST /v1/turn-credentials` — authenticate/rate-limit a client and issue
  short-lived Cloudflare credentials from the server side;
- `POST /v1/servers/heartbeat` and `GET /v1/servers` for dedicated servers;
- `DELETE /v1/sessions/{id}` for cleanup/revocation.

Because Steam identity is unavailable, begin with random device/session keys and
invite codes. Before public matchmaking, add an account or signed-device model,
rate limits, bans, protocol/build compatibility checks, and abuse reporting.

Candidate signaling must never be trusted as authorization. Bind a random
128-bit join secret and reservation cookie to every session, and require the
normal Source challenge/response before allocating a player slot.

## Voice and lobby traffic

Remove or compile out the existing `ISteamNetworking` calls in
`sys_session.cpp`. Replace lobby control messages with the signaling WebSocket.
If profiling shows that high-frequency session messages cannot use the control
plane, add a small framed reliable channel over the selected ICE path with
sequence numbers, acknowledgements, retransmission, duplicate suppression, and
strict size/rate limits.

Gameplay voice should remain on the Source netchannel if the engine path already
supports it. Otherwise defer voice until gameplay is stable; do not recreate
Steam Voice or send microphone data through lobby signaling.

TURN should relay the selected gameplay UDP flow, not create a second unrelated
Steam-compatible P2P API. No `ISteamNetworking` compatibility adapter should be
shipped on PS4.

## Security and operational requirements

- Keep Cloudflare secrets server-side.
- Issue least-lifetime credentials and rate-limit issuance per device/IP/session.
- Cap concurrent allocations and bytes per session.
- Validate Source connectionless packets before expensive processing.
- Add replay-resistant session tokens and reservation cookies.
- Never accept a lobby-provided endpoint as proof of identity.
- Log route type without logging TURN credentials or player secrets.
- Track direct-vs-relay success, candidate timing, RTT, loss, allocation
  refreshes, credential failures, relay bytes, and unexpected disconnects.
- TURN hides a host's direct address only when relay-only policy is selected.
  Normal ICE direct candidates expose peer addresses.
- Provide a relay-only privacy option only after cost and latency are measured.

## Implementation sequence

1. Port `net_ws.cpp` socket calls to OpenOrbis and pass UDP loopback tests.
2. Complete offline listen-server operation and bot-match acceptance.
3. Add direct LAN connect and two-console soak tests.
4. Build the Linux/community dedicated server with the same protocol/content
   version and validate PS4-to-server play.
5. Add a minimal server directory and heartbeat protocol.
6. Define `IPs4DatagramTransport`; move native UDP behind it with byte-for-byte
   and timing regression tests.
7. Cross-compile and unit-test the selected ICE library independently.
8. Add STUN candidate discovery and candidate signaling; prove direct
   NAT-punched sessions before enabling TURN.
9. Add Cloudflare TURN/UDP fallback with short-lived backend-issued credentials.
10. Add TURN/TCP/TLS fallback only if real network testing justifies it.
11. Replace Steam lobby P2P control paths with `IKisakSessionTransport` and the
    open signaling/session layer; remove PS4 `ISteamNetworking` references.
12. Add browser/admin observability, abuse controls, credential revocation, and
    relay-cost alerts before public matchmaking.

## Test matrix

- PS4 loopback listen server.
- Two PS4s on one LAN, direct address and discovery.
- PS4 to public Linux dedicated server.
- Full-cone, restricted, port-restricted, symmetric NAT, double NAT, and CGNAT.
- Direct ICE success with TURN disabled.
- Forced Cloudflare TURN/UDP relay.
- Forced TURN/TCP and TURN/TLS fallback.
- Relay credential expiry and in-session refresh.
- Host migration is explicitly unsupported initially; verify clean termination.
- Packet loss, reorder, duplication, MTU fragmentation, and 30-minute soak.
- Suspend/resume, cable loss, Wi-Fi reconnect, DNS failure, signaling outage,
  TURN outage, and backend restart.
- Cross-version/content mismatch rejection.
- Malformed STUN/TURN/signaling and Source connectionless packet fuzzing.

## Release gates

LAN release:

- two consoles complete a match with no memory growth or netchannel timeout;
- reconnect and clean shutdown work;
- no Steam/PSN dependency is present.

Dedicated-server Internet release:

- public server discovery and direct UDP work across common home networks;
- server build/content compatibility and challenge authorization are enforced;
- 60-minute soak is stable.

Player-hosted Internet release:

- ICE selects direct routes when available and relays only when required;
- symmetric-NAT/CGNAT cases succeed through Cloudflare TURN/UDP;
- expired credentials refresh or disconnect cleanly;
- relay latency, loss, bandwidth, and cost are observable and bounded;
- backend compromise cannot expose the Cloudflare TURN key to clients.

## Final decision

Cloudflare STUN/TURN is a viable optional component for PS4 player-hosted
multiplayer. The safest and fastest route to usable multiplayer is nevertheless
LAN plus public dedicated servers over the existing Source UDP/netchannel
stack. Add Cloudflare-backed ICE later as a transport adapter and fallback,
with a separate signaling and credential backend. Do not make TURN a dependency
for offline, LAN, or dedicated-server play.
