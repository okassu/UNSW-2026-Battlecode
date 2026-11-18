// Dragon bot. Every dragon runs its own copy of this process, so everything
// below is per-dragon memory; teammates only share what sonar carries.
//
// Turn priority:
//   1. lunge into a bigger enemy head (head-on kills both)
//   2. endgame: feeders walk to the carrier and sacrifice beside it
//   3. split while the swarm is growing
//   4. move: the best target reachable through a *safe* first step, where
//      safe = time-aware flood fill from that step finds room for the body
//   5. trapped: U-turn split (the old tail leaves as a new dragon)
#include "helper.hpp"
#include "maplib.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <functional>
#include <unordered_map>
#include <random>
#include <string>
#include <vector>

using namespace unswbc;

namespace {

// ---------------------------------------------------------------- tuning
// Defaults are what the judge runs. Local tests can override any of them with
// an environment variable of the same name (the judge sets none).
int param(char const* name, int fallback) {
    char const* e = std::getenv(name);
    return e ? std::atoi(e) : fallback;
}
int SPLIT_LEN = param("SPLIT_LEN", 4);                   // grower splits at this length (2+2, as SSS/Vibing++ do)
int SPLIT_UNTIL = param("SPLIT_UNTIL", 380);             // no splitting after this round
int POP_RESERVE = param("POP_RESERVE", 2);               // keep this many unit slots free
int AREA_PER_DRAGON = param("AREA_PER_DRAGON", 10);      // population cap = map area / this
int SPLIT_CROWD = param("SPLIT_CROWD", 16);              // no split with more ally segments than this in view
int GUARD_MIN_LEN = param("GUARD_MIN_LEN", 8);           // only a carrier this long is worth an even trade
int CONSOLIDATE_FROM = param("CONSOLIDATE_FROM", 360);   // feeders start walking to the carrier
int FEED_RADIUS = param("FEED_RADIUS", 3);               // sacrifice this close to the carrier
int FEED_MAX_PEARLS = param("FEED_MAX_PEARLS", 2);       // ...unless it already has this many pearls near it
int FEED_MAX_PEARLS_END = param("FEED_MAX_PEARLS_END", 99); // same, during the endgame
int FEED_FROM = param("FEED_FROM", 1000);                // hive feeding starts
int FEEDER_MOD = param("FEEDER_MOD", 3);                 // 1 in this many dragons is a dedicated feeder
int FEED_NEAR = param("FEED_NEAR", 6);                   // anyone this close to the carrier feeds
int FEED_MIN_UNITS = param("FEED_MIN_UNITS", 6);         // hive feeding stops below this many dragons
int MIN_UNITS_TO_TRADE = param("MIN_UNITS_TO_TRADE", 4); // never lunge below this many team dragons
int INSURANCE_MOD = param("INSURANCE_MOD", 0);           // 1 in this many non-carriers never feeds (0 = none)
int LOW_UNITS = param("LOW_UNITS", 3);                   // team this small: every dragon is precious
int THREAT_LOW = param("THREAT_LOW", 300000);            // penalty for ending in enemy reach when precious
int THREAT_BIGGER = param("THREAT_BIGGER", 300000);      // ...when a shorter enemy could trade into us
int GUARD_RADIUS = param("GUARD_RADIUS", 5);             // enemy heads this near the carrier get rammed
int HUNT_MIN_LEN = param("HUNT_MIN_LEN", 8);             // visible enemies this long attract our small dragons
int HUNTER_MOD = param("HUNTER_MOD", 6);                 // 1 in this many small dragons chases the enemy carrier (0 = none)
int HUNT_FROM = param("HUNT_FROM", 150);                 // ...from this round
int HUNT_TARGET_LEN = param("HUNT_TARGET_LEN", 12);      // ...if it is at least this long
int INTEL_TTL = param("INTEL_TTL", 15);                  // rounds an enemy-carrier sighting stays useful
int CAUTIOUS_LEN = param("CAUTIOUS_LEN", 6);             // dragons this long stay out of enemy reach
int RIM_LEN = param("RIM_LEN", 0);                       // assumed length of an enemy leaving our window (0 = visible count)
int FLEE_WEIGHT = param("FLEE_WEIGHT", 30000);           // carrier: score per tile of distance from enemy heads
int CARRIER_FROM = param("CARRIER_FROM", 60);            // before this nobody is the carrier
int CARRIER_MIN_LEN = param("CARRIER_MIN_LEN", 5);       // with no carrier heard, only this long a dragon claims it
int KING = param("KING", 1);                             // each team's first dragon is the default carrier
int TAKEOVER_MARGIN = param("TAKEOVER_MARGIN", 3);       // be this much longer than the carrier to replace it
int BEACON_TTL = param("BEACON_TTL", 12);                // rounds a carrier beacon stays trusted
constexpr int INF = 1 << 29;

constexpr int DX[4] = {0, 1, 0, -1};
constexpr int DY[4] = {-1, 0, 1, 0};
constexpr char DC[4] = {'N', 'E', 'S', 'W'};

int W, H, N;
int myId;
Team myTeam = Team::A;
std::mt19937 rng;

inline int idx(int x, int y) { return y * W + x; }
inline int px(int i) { return i % W; }
inline int py(int i) { return i / W; }
inline int step(int i, int d) {
    int x = px(i) + DX[d], y = py(i) + DY[d];
    x = (x + W) % W;
    y = (y + H) % H;
    return idx(x, y);
}
inline int dirIndex(Direction d) {
    switch (d.value) {
    case Direction::NORTH: return 0;
    case Direction::EAST: return 1;
    case Direction::SOUTH: return 2;
    default: return 3;
    }
}
inline int wrapDist(int a, int b, int size) {
    int d = std::abs(a - b);
    return std::min(d, size - d);
}
inline int manhattan(int a, int b) { return wrapDist(px(a), px(b), W) + wrapDist(py(a), py(b), H); }

// ---------------------------------------------------------------- memory
struct Memory {
    std::vector<uint8_t> blocked;   // bit d: can't cross the edge (kelp, or portal: treated as wall)
    std::vector<uint8_t> kelp;      // bit d: kelp specifically
    std::vector<int> lastSeen;      // round the tile was last in vision, -1 never
    std::vector<int> spawnAt;       // absolute round of next spawn attempt, -1 never spawns
    std::vector<uint8_t> pearl;     // pearl present when last seen
    std::vector<float> exploreBias; // per-dragon random preference for regions
    std::vector<int> gapLo, gapHi;  // spawn gap range per tile (from the map library), -1 unknown
    void init() {
        gapLo.assign(N, -1);
        gapHi.assign(N, -1);
        blocked.assign(N, 0);
        kelp.assign(N, 0);
        lastSeen.assign(N, -1);
        spawnAt.assign(N, -2); // -2 unknown
        pearl.assign(N, 0);
        exploreBias.assign(N, 1.f);
        std::uniform_real_distribution<float> u(0.4f, 1.6f);
        int const bw = std::max(1, W / 4), bh = std::max(1, H / 4);
        std::vector<float> blocks(16);
        for (auto& b : blocks) b = u(rng);
        for (int i = 0; i < N; i++)
            exploreBias[i] = blocks[std::min(3, px(i) / bw) * 4 + std::min(3, py(i) / bh)];
    }
} mem;

std::vector<int> headHist; // head positions, newest first, one per step taken

// ---------------------------------------------------------------- sonar
// 64-bit message: [tag 12][type 2][round 9][id 13][length 10][x 6][y 6][spare 6]
constexpr uint64_t TAG = 0xB7D;
enum MsgType { MSG_CARRIER = 1, MSG_ENEMY = 2 };
struct Msg {
    int type, round, id, length, x, y;
};
uint64_t teamSalt() { return myTeam == Team::A ? 0x5A17C0DE31ULL : 0x2B0BCAFE77ULL; }
uint64_t encode(Msg const& m) {
    uint64_t v = TAG;
    v = (v << 2) | (m.type & 3);
    v = (v << 9) | (m.round & 511);
    v = (v << 13) | (m.id & 8191);
    v = (v << 10) | std::min(m.length, 1023);
    v = (v << 6) | (m.x & 63);
    v = (v << 6) | (m.y & 63);
    v <<= 6;
    // keyed checksum in the spare bits: enemy traffic (even from a copy of
    // this bot playing the other side) is rejected
    uint64_t h = ((v >> 6) ^ teamSalt()) * 0x9E3779B97F4A7C15ULL;
    v |= (h >> 58) & 63;
    return v;
}
bool decode(uint64_t v, Msg& m) {
    if ((v >> 52) != TAG) return false;
    uint64_t h = (((v & ~63ULL) >> 6) ^ teamSalt()) * 0x9E3779B97F4A7C15ULL;
    if (((h >> 58) & 63) != (v & 63)) return false;
    uint64_t b = v >> 6;
    m.y = b & 63; b >>= 6;
    m.x = b & 63; b >>= 6;
    m.length = b & 1023; b >>= 10;
    m.id = b & 8191; b >>= 13;
    m.round = b & 511; b >>= 9;
    m.type = b & 3;
    return true;
}

struct Carrier {
    int id = -1, length = 0, pos = -1, round = -1000;
} carrier; // best-known team carrier (longest dragon heard of)
struct EnemyIntel {
    int length = 0, pos = -1, round = -1000;
} enemyBig; // biggest enemy heard of

// ---------------------------------------------------------------- map library
std::vector<int> mapCands; // library maps still consistent with everything seen
int mapKnown = -1;         // library index once identified
bool debugOn = false;

inline int libKelp(maplib::MapInfo const& m, int i) {
    char c = m.kelp[i];
    return c <= '9' ? c - '0' : c - 'a' + 10;
}
inline int libPortal(maplib::MapInfo const& m, int i) {
    char c = m.portal[i];
    return c <= '9' ? c - '0' : c - 'a' + 10;
}
inline int libGapIndex(maplib::MapInfo const& m, int i) {
    char c = m.gaps[i];
    if (c <= '9') return c - '0';
    if (c <= 'Z') return c - 'A' + 36;
    return c - 'a' + 10;
}

void initMapCands() {
    for (int k = 0; k < maplib::mapCount; k++)
        if (maplib::maps[k].w == W && maplib::maps[k].h == H) mapCands.push_back(k);
}

void applyMap(int k) {
    auto const& m = maplib::maps[k];
    for (int i = 0; i < N; i++) {
        mem.kelp[i] = libKelp(m, i);
        mem.blocked[i] = libKelp(m, i) | libPortal(m, i);
        auto const& g = m.palette[libGapIndex(m, i)];
        mem.gapLo[i] = g.lo;
        mem.gapHi[i] = g.hi;
        if (g.hi == 0) mem.spawnAt[i] = -1;
    }
    mapKnown = k;
}

// Drop candidates that disagree with this turn's vision.
void checkMapCands(Controller& ct) {
    if (mapCands.empty()) return;
    std::vector<int> keep;
    for (int k : mapCands) {
        auto const& m = maplib::maps[k];
        bool ok = true;
        for (auto const& tile : ct.get_tiles()) {
            int const i = idx(tile.position.x, tile.position.y);
            int kb = libKelp(m, i), pb = libPortal(m, i);
            for (int d = 0; d < 4 && ok; d++) {
                auto t = tile.edges[d].edge_type;
                bool isK = t == EdgeType::KELP, isP = t == EdgeType::PORTAL;
                if (isK != bool(kb & (1 << d)) || isP != bool(pb & (1 << d))) ok = false;
            }
            bool never = tile.get_pearl_time() < 0;
            if (never != (m.palette[libGapIndex(m, i)].hi == 0)) ok = false;
            if (!ok) break;
        }
        if (ok) keep.push_back(k);
    }
    if (debugOn && keep.empty()) ct.output_log("MAPLIB: no candidate matches");
    if (mapKnown >= 0 && (keep.size() != 1 || keep[0] != mapKnown)) {
        // Wrong map after all: forget library walls, relearn from vision.
        mapKnown = -1;
        std::fill(mem.blocked.begin(), mem.blocked.end(), 0);
        std::fill(mem.kelp.begin(), mem.kelp.end(), 0);
        std::fill(mem.gapLo.begin(), mem.gapLo.end(), -1);
        std::fill(mem.gapHi.begin(), mem.gapHi.end(), -1);
    }
    mapCands = keep;
    if (mapKnown < 0 && mapCands.size() == 1) {
        applyMap(mapCands[0]);
        if (debugOn) ct.output_log("MAPID", maplib::maps[mapKnown].name);
    }
}

// Tiles reachable from `from` without portals, using walls known so far.
int reachableArea(int from) {
    std::vector<uint8_t> seen(N, 0);
    std::vector<int> q{from};
    seen[from] = 1;
    for (size_t qi = 0; qi < q.size(); qi++)
        for (int d = 0; d < 4; d++) {
            if (mem.blocked[q[qi]] & (1 << d)) continue;
            int n = step(q[qi], d);
            if (!seen[n]) { seen[n] = 1; q.push_back(n); }
        }
    return (int)q.size();
}

// ---------------------------------------------------------------- per-turn state
struct Turn {
    int round, L, units;
    int head;
    int headDir;
    std::vector<int> freeAt;      // first step index at which the tile can be entered
    std::vector<int> body;        // own body, head first (known part)
    std::vector<uint8_t> inVision;
    std::vector<int> allyHeads, enemyHeads;
    std::vector<int> enemyHeadLen; // visible segment count, a lower bound
    std::vector<int> enemyReachLen; // length assumed for sprint reach (4 if it leaves our window)
    std::vector<uint8_t> threat;   // cells an enemy head could reach next turn
    std::vector<int> threatLen;    // shortest enemy (visible length) that can reach the cell
    std::vector<uint8_t> threatFar; // reachable assuming any enemy can sprint 3
    std::vector<uint8_t> allyNear; // cells adjacent to an ally head (crowding)
    std::vector<int> headDist;     // steps for the nearest other head to reach a tile
} T;

// Walk own segments from the head using body facings.
std::vector<int> ownChain(Controller& ct) {
    std::vector<int> chain{T.head};
    std::vector<int> ownFace(N, -1);
    for (auto const& tile : ct.get_tiles()) {
        auto const* p = tile.get_dragon();
        if (p && p->get_id() == myId && !p->is_head())
            ownFace[idx(tile.position.x, tile.position.y)] = dirIndex(p->get_dir());
    }
    std::vector<uint8_t> used(N, 0);
    used[T.head] = 1;
    int cur = T.head;
    while ((int)chain.size() < T.L) {
        int next = -1;
        for (int d = 0; d < 4; d++) {
            int n = step(cur, d);
            if (ownFace[n] >= 0 && !used[n] && step(n, ownFace[n]) == cur) {
                next = n;
                break;
            }
        }
        if (next < 0) break;
        used[next] = 1;
        chain.push_back(next);
        cur = next;
    }
    return chain;
}

void readVision(Controller& ct) {
    T.inVision.assign(N, 0);
    T.freeAt.assign(N, 0);
    T.allyHeads.clear();
    T.enemyHeads.clear();
    T.enemyHeadLen.clear();
    std::vector<int> segCount(0);
    std::unordered_map<int, int> countById;

    for (auto const& tile : ct.get_tiles()) {
        int const i = idx(tile.position.x, tile.position.y);
        T.inVision[i] = 1;
        mem.lastSeen[i] = T.round;
        mem.pearl[i] = tile.has_pearl();
        int const pt = tile.get_pearl_time();
        mem.spawnAt[i] = pt < 0 ? -1 : T.round + pt;
        for (int d = 0; d < 4; d++) {
            auto const& e = tile.edges[d];
            uint8_t const bit = 1 << d;
            if (e.edge_type == EdgeType::KELP) {
                mem.kelp[i] |= bit;
                mem.blocked[i] |= bit;
            } else if (e.edge_type == EdgeType::PORTAL) {
                mem.blocked[i] |= bit;
            } else {
                mem.blocked[i] &= ~bit;
                mem.kelp[i] &= ~bit;
            }
        }
        if (auto const* p = tile.get_dragon())
            countById[p->get_id()]++;
    }
    // The edge between two tiles is reported from both sides; mirror blocked bits
    // onto the neighbour so a tile at the vision rim knows its outer edges.
    for (auto const& tile : ct.get_tiles()) {
        int const i = idx(tile.position.x, tile.position.y);
        for (int d = 0; d < 4; d++) {
            int n = step(i, d);
            int od = (d + 2) & 3;
            if (mem.blocked[i] & (1 << d)) mem.blocked[n] |= 1 << od;
            if (mem.kelp[i] & (1 << d)) mem.kelp[n] |= 1 << od;
        }
    }

    // A dragon with a segment on the window's rim may continue out of view,
    // so it may be longer (and sprint further) than its visible count says.
    std::unordered_map<int, int> rimById;
    for (auto const& tile : ct.get_tiles())
        if (auto const* p = tile.get_dragon()) {
            int const dx = wrapDist(tile.position.x, px(T.head), W), dy = wrapDist(tile.position.y, py(T.head), H);
            if (dx == Constants::VISION_RADIUS || dy == Constants::VISION_RADIUS) rimById[p->get_id()] = 1;
        }
    T.enemyReachLen.clear();
    for (auto const& tile : ct.get_tiles()) {
        auto const* p = tile.get_dragon();
        if (!p) continue;
        int const i = idx(tile.position.x, tile.position.y);
        if (p->get_id() == myId) continue; // own body handled below
        T.freeAt[i] = INF;
        if (p->is_head()) {
            if (p->get_team() == myTeam) T.allyHeads.push_back(i);
            else {
                T.enemyHeads.push_back(i);
                T.enemyHeadLen.push_back(countById[p->get_id()]);
                T.enemyReachLen.push_back(rimById.count(p->get_id()) ? std::max(RIM_LEN, countById[p->get_id()])
                                                                     : countById[p->get_id()]);
            }
        }
    }

    // Own body: facing chain from vision, extended with remembered head positions.
    std::vector<int> chain = ownChain(ct);
    if (headHist.empty() || headHist[0] != T.head) headHist = chain;
    T.body.clear();
    for (int i = 0; i < T.L; i++) {
        if (i < (int)chain.size()) T.body.push_back(chain[i]);
        else if (i < (int)headHist.size()) T.body.push_back(headHist[i]);
        else break;
    }
    // Segment i is removed after L-i moves; the tail blocks the first step.
    for (int i = 0; i < (int)T.body.size(); i++)
        T.freeAt[T.body[i]] = std::max(T.freeAt[T.body[i]], T.L - i + 1);
    // Own segments we saw but could not place in order: assume they stay a while.
    if ((int)T.body.size() < T.L) {
        int const unplaced = T.L + 1 - (int)T.body.size();
        for (auto const& tile : ct.get_tiles())
            if (auto const* p = tile.get_dragon(); p && p->get_id() == myId) {
                int const i = idx(tile.position.x, tile.position.y);
                if (T.freeAt[i] == 0) T.freeAt[i] = std::max(2, unplaced);
            }
    }

    // Threat: tiles a visible enemy head can reach next turn by moving or
    // sprinting (a length-L dragon can take at most L-1 steps, we assume <= 3).
    // threatLen = the shortest such enemy: one shorter than us gains by a head-on.
    // Visible length undercounts a dragon leaving our window, so threatFar
    // assumes every enemy can sprint the full 3 steps (used by the carrier).
    T.threat.assign(N, 0);
    T.threatFar.assign(N, 0);
    T.threatLen.assign(N, INF);
    for (size_t k = 0; k < T.enemyHeads.size(); k++) {
        int const e = T.enemyHeads[k], le = T.enemyReachLen[k];
        int const reach = std::max(1, std::min(le - 1, 3));
        int const farReach = 3;
        std::vector<std::pair<int, int>> q{{e, 0}};
        std::vector<int> seen{e};
        for (size_t qi = 0; qi < q.size(); qi++) {
            auto [c, dd] = q[qi];
            if (dd >= farReach) continue;
            for (int d = 0; d < 4; d++) {
                if (mem.blocked[c] & (1 << d)) continue;
                int n = step(c, d);
                if (std::find(seen.begin(), seen.end(), n) != seen.end()) continue;
                seen.push_back(n);
                T.threatFar[n] = 1;
                if (T.freeAt[n] == 0) q.push_back({n, dd + 1});
                if (dd >= reach) continue;
                T.threat[n] = 1;
                // Pessimistic both ways: reach assumes the enemy may be long, while
                // "is it shorter than us" (so it gains by a head-on) uses what we see.
                T.threatLen[n] = std::min(T.threatLen[n], T.enemyHeadLen[k]);
            }
        }
    }

    // How soon could any other visible head reach each tile (ignoring bodies)?
    T.headDist.assign(N, INF);
    {
        std::vector<int> q;
        for (int h : T.allyHeads) { T.headDist[h] = 0; q.push_back(h); }
        for (int h : T.enemyHeads) { T.headDist[h] = 0; q.push_back(h); }
        for (size_t qi = 0; qi < q.size(); qi++) {
            int c = q[qi];
            if (T.headDist[c] >= 12) continue;
            for (int d = 0; d < 4; d++) {
                if (mem.blocked[c] & (1 << d)) continue;
                int n = step(c, d);
                if (T.headDist[n] != INF || T.freeAt[n] >= INF) continue;
                T.headDist[n] = T.headDist[c] + 1;
                q.push_back(n);
            }
        }
    }
    T.allyNear.assign(N, 0);
    for (int a : T.allyHeads)
        for (int d = 0; d < 4; d++) T.allyNear[step(a, d)] = 1;
}

// Can the head enter tile n at step t (1-based) by crossing edge d of tile from?
inline bool enterable(int from, int d, int n, int t) {
    if (mem.blocked[from] & (1 << d)) return false;
    return T.freeAt[n] <= t;
}

// ---------------------------------------------------------------- evaluation
bool feederMode = false;       // this dragon is walking to the carrier to be eaten
std::vector<float> huntVal;    // tiles next to big enemy heads, for would-be attackers
std::vector<int> carrierDist;  // wall-aware steps to the carrier's head

float tileValue(int c, int t) {
    int const R = T.round;
    // Pearls near the carrier are the carrier's; a feeder leaves them.
    if (feederMode && carrierDist[c] <= 4) return 0.f;
    if (!huntVal.empty() && huntVal[c] > 0) return huntVal[c];
    if (T.inVision[c]) {
        if (mem.pearl[c]) return 10.f;
        int sp = mem.spawnAt[c];
        if (sp >= 0 && sp <= R + t + 1) return 5.f;         // spawns by the time we arrive
        if (sp >= 0 && sp <= R + t + 4) return 2.f;
        return 0.f;
    }
    int const hi = mem.gapHi[c];
    if (hi == 0) return 0.f; // library says this tile never spawns
    int age = mem.lastSeen[c] < 0 ? R : R - mem.lastSeen[c];
    if (mem.lastSeen[c] >= 0 && mem.pearl[c]) return age < 25 ? 7.f : 4.f;
    int sp = mem.spawnAt[c];
    if (sp == -1) return 0.f;
    if (hi > 0) {
        // Known spawn rate: chance a pearl is sitting there by arrival.
        float avg = 0.5f * (mem.gapLo[c] + hi);
        float since = sp >= 0 ? float(R + t - sp) : float(age + t);
        if (sp >= 0 && sp > R + t) return 0.f;
        float p = std::min(1.f, (since + (sp >= 0 ? avg : 0.f)) / std::max(1.f, avg));
        return (mem.lastSeen[c] < 0 ? 0.4f * mem.exploreBias[c] : 0.f) + 4.5f * p;
    }
    if (mem.lastSeen[c] < 0) return 1.2f * mem.exploreBias[c]; // never seen, map unknown
    if (sp >= 0 && sp <= R + t) return 4.f;
    if (age > 60) return 0.6f * mem.exploreBias[c];
    return 0.f;
}

struct Eval {
    bool legal = false;
    int space = 0;        // time-aware reachable cells (capped)
    float value = 0;      // best target value / distance
    int threat = 0;
    bool headOn = false;
    bool survive = false; // survival path exists
    bool strict = false;  // ... even if every other head races us for tiles
    bool safe(int need) const { return legal && survive && space >= need; }
};

std::vector<int> bfsDist, bfsQueue;

// Flood from the first step dest (reached at step 1), time-aware.
Eval evaluate(int dest, int need, bool wantValue) {
    Eval e;
    e.legal = true;
    bfsDist.assign(N, -1);
    bfsQueue.clear();
    bfsDist[dest] = 1;
    bfsQueue.push_back(dest);
    float best = 0;
    size_t qi = 0;
    int const cap = wantValue ? N : need;
    while (qi < bfsQueue.size() && (int)bfsQueue.size() < cap + 1) {
        int c = bfsQueue[qi++];
        int t = bfsDist[c];
        if (wantValue) {
            float v = tileValue(c, t);
            if (v > 0) {
                // spread out: tiles next to other ally heads are someone else's
                bool allyCloser = false;
                for (int a : T.allyHeads)
                    if (manhattan(a, c) + 1 < t) { allyCloser = true; break; }
                if (allyCloser) v *= 0.35f;
                float s = v / (float)(t + 1);
                if (s > best) best = s;
            }
        }
        for (int d = 0; d < 4; d++) {
            int n = step(c, d);
            if (bfsDist[n] >= 0) continue;
            if (!enterable(c, d, n, t + 1)) continue;
            bfsDist[n] = t + 1;
            bfsQueue.push_back(n);
        }
    }
    e.space = (int)bfsQueue.size();
    e.value = best;
    e.threat = T.threat[dest];
    return e;
}

// Survival: is there a self-avoiding path of `depth` steps starting at dest
// (step 1), or one that steps onto a tile our own body has vacated (tail
// chasing, which can go on forever)? A flood fill alone happily counts a
// dead-end corridor as room; a snake can't turn round in one.
std::vector<int> dfsMark;
int dfsStamp = 0, dfsNodes = 0;
bool dfsStrict = false; // other heads advance one tile per step and claim what they reach first
constexpr int DFS_BUDGET = 4000;

bool dfsPath(int c, int t, int depth) {
    if (t >= depth) return true;
    if (++dfsNodes > DFS_BUDGET) return true; // too many branches: plenty of room
    // try the neighbour with the most exits first: finds long paths fast
    int order[4], exits[4], cnt = 0;
    for (int d = 0; d < 4; d++) {
        int n = step(c, d);
        if (mem.blocked[c] & (1 << d)) continue;
        if (dfsMark[n] == dfsStamp) continue;
        if (T.freeAt[n] > t + 1) continue;
        if (dfsStrict && T.headDist[n] < t + 1) continue;
        if (T.freeAt[n] >= 2 && T.freeAt[n] < INF) return true; // onto our vacated body
        int ex = 0;
        for (int d2 = 0; d2 < 4; d2++) {
            int m = step(n, d2);
            if (!(mem.blocked[n] & (1 << d2)) && dfsMark[m] != dfsStamp && T.freeAt[m] <= t + 2) ex++;
        }
        order[cnt] = n;
        exits[cnt++] = ex;
    }
    for (int i = 0; i < cnt; i++)
        for (int j = i + 1; j < cnt; j++)
            if (exits[j] > exits[i]) std::swap(exits[i], exits[j]), std::swap(order[i], order[j]);
    for (int i = 0; i < cnt; i++) {
        int n = order[i];
        dfsMark[n] = dfsStamp;
        if (dfsPath(n, t + 1, depth)) return true;
        dfsMark[n] = 0;
    }
    return false;
}

bool survives(int dest, int depth, bool strict) {
    dfsStrict = strict;
    if ((int)dfsMark.size() != N) dfsMark.assign(N, 0);
    ++dfsStamp;
    dfsNodes = 0;
    dfsMark[dest] = dfsStamp;
    return dfsPath(dest, 1, depth);
}

// Room a U-turn child would have: it is born with its head on our tail and
// our whole body behind it, so flood from the tail with the body as walls.
int tailRoom(int cap) {
    if ((int)T.body.size() < T.L) return 0; // tail position unknown
    std::vector<uint8_t> seen(N, 0);
    for (int b : T.body) seen[b] = 1;
    for (size_t i = 0; i < (size_t)N; i++)
        if (T.freeAt[i] >= INF) seen[i] = 1; // other dragons
    std::vector<int> q;
    int const tail = T.body.back();
    for (int d = 0; d < 4; d++) {
        if (mem.blocked[tail] & (1 << d)) continue;
        int n = step(tail, d);
        if (!seen[n]) { seen[n] = 1; q.push_back(n); }
    }
    for (size_t qi = 0; qi < q.size() && (int)q.size() < cap; qi++)
        for (int d = 0; d < 4; d++) {
            if (mem.blocked[q[qi]] & (1 << d)) continue;
            int n = step(q[qi], d);
            if (!seen[n]) { seen[n] = 1; q.push_back(n); }
        }
    return (int)q.size();
}

// ---------------------------------------------------------------- actions
std::string moveStr;

void recordMove(std::vector<int> const& dirs) {
    int cur = T.head;
    for (int d : dirs) {
        cur = step(cur, d);
        headHist.insert(headHist.begin(), cur);
    }
    if ((int)headHist.size() > T.L + 2) headHist.resize(T.L + 2);
}

// Sprint up to `maxSteps` into an enemy head. Returns the path if one exists.
bool findLunge(int target, int maxSteps, std::vector<int>& path) {
    // tiny DFS over paths of length <= maxSteps
    std::vector<int> cur;
    std::function<bool(int, int)> dfs = [&](int at, int t) -> bool {
        if (t > maxSteps) return false;
        for (int d = 0; d < 4; d++) {
            if (mem.blocked[at] & (1 << d)) continue;
            int n = step(at, d);
            cur.push_back(d);
            if (n == target) { path = cur; return true; }
            if (T.freeAt[n] <= t && dfs(n, t + 1)) return true;
            cur.pop_back();
        }
        return false;
    };
    return dfs(T.head, 1);
}

} // namespace

int main() {
    auto [ct, game] = init();
    myId = ct.get_id();
    myTeam = ct.get_team();
    W = game.width;
    H = game.height;
    N = W * H;
    uint32_t seed = 0x5eed1234u;
    if (char const* e = std::getenv("DRAGON_SEED")) seed ^= (uint32_t)std::strtoul(e, nullptr, 10) * 0x9E3779B1u;
    rng.seed(seed ^ (uint32_t)(myId * 2654435761u));
    mem.init();
    bool born = true;
    int reachArea = -1;     // tiles reachable without portals, once the map is known
    bool handedOver = false; // this dragon was the carrier and U-turned: the child carries on
    debugOn = std::getenv("DRAGON_DEBUG") != nullptr;
    initMapCands();

    while (update(ct, game)) {
        T.round = game.get_round_num();
        T.L = ct.get_length();
        T.units = ct.get_unit_count();
        T.head = idx(ct.get_position().x, ct.get_position().y);
        T.headDir = dirIndex(ct.get_dir());
        checkMapCands(ct);
        readVision(ct);

        // --- sonar in
        for (uint64_t raw : ct.get_sonar_messages()) {
            // SSS relays its longest dragon in plain text: tag 0xB3C0, id, length,
            // x, y and a timestamp of about twice the round (checked against ladder logs).
            if ((raw >> 48) == 0xB3C0) {
                int const len = (raw >> 31) & 127, x = (raw >> 25) & 63, y = (raw >> 19) & 63;
                int const when = std::min(T.round, (int)((raw >> 9) & 1023) / 2);
                if (x < W && y < H && T.round - when <= INTEL_TTL &&
                    (when > enemyBig.round || len > enemyBig.length))
                    enemyBig = {len, idx(x, y), when};
                continue;
            }
            Msg m;
            if (!decode(raw, m)) continue;
            int r = m.round; // 9 bits of round, reconstruct nearest
            int full = (T.round & ~511) | r;
            if (full > T.round) full -= 512;
            if (T.round - full > 60) continue;
            if (m.type == MSG_CARRIER) {
                bool fresher = full > carrier.round && m.id == carrier.id;
                bool better = m.length > carrier.length || (m.length == carrier.length && m.id < carrier.id);
                bool stale = T.round - carrier.round > BEACON_TTL;
                if (fresher || ((better || stale) && full >= T.round - BEACON_TTL)) {
                    carrier = {m.id, m.length, idx(m.x % W, m.y % H), full};
                }
            } else if (m.type == MSG_ENEMY) {
                if (T.round - full <= INTEL_TTL && (full > enemyBig.round || m.length > enemyBig.length))
                    enemyBig = {m.length, idx(m.x % W, m.y % H), full};
            }
        }
        if (T.round - carrier.round > BEACON_TTL) carrier = Carrier{};
        // Am I the carrier? Longest known, ties to the lower id.
        // Early on everyone splits; after that the longest dragon heard of keeps growing.
        // The king (each team's first dragon, id 0 or 1) is the carrier by
        // default, so the role doesn't scatter over dozens of 3-cell dragons that
        // each stop splitting and then die. Anyone longer takes over; with no
        // carrier heard, only a dragon that has already grown claims the role.
        bool const king = KING && myId <= 1;
        bool const iAmCarrier = !handedOver && T.round >= CARRIER_FROM &&
                                (carrier.id == myId || T.L >= carrier.length + TAKEOVER_MARGIN ||
                                 (carrier.id < 0 && (king || T.L >= CARRIER_MIN_LEN)));
        if (iAmCarrier) carrier = {myId, T.L, T.head, T.round};
        // A carrier we can see: trust our eyes over the beacon.
        if (!iAmCarrier && carrier.id >= 0)
            for (auto const& tile : ct.get_tiles())
                if (auto const* p = tile.get_dragon(); p && p->is_head() && p->get_id() == carrier.id)
                    carrier.pos = idx(tile.position.x, tile.position.y);

        // Sealed regions (stronghold, trauma) hold far fewer dragons than the map
        // area suggests; overcrowding them only feeds self-inflicted deaths.
        if (mapKnown >= 0 && reachArea < 0) reachArea = reachableArea(T.head);
        int const popCap = std::min(ct.unit_limit - POP_RESERVE, std::max(4, (reachArea < 0 ? N : reachArea) / AREA_PER_DRAGON));
        bool const endgame = T.round >= CONSOLIDATE_FROM;
        bool const insurance = !iAmCarrier && INSURANCE_MOD > 0 && myId % INSURANCE_MOD == 1;
        bool const carrierKnown = !iAmCarrier && carrier.id >= 0 && carrier.pos >= 0;
        int const carrierD = carrierKnown ? manhattan(T.head, carrier.pos) : INF;
        // Hive feeding: from FEED_FROM a share of the swarm (and anyone already
        // close) keeps the carrier eating; in the endgame everyone does.
        bool const hiveFeeder = carrierKnown && T.round >= FEED_FROM && carrier.length > T.L + 2 &&
                                T.units > FEED_MIN_UNITS &&
                                ((FEEDER_MOD > 0 && myId % FEEDER_MOD == 0) || carrierD <= FEED_NEAR);
        // Never feed a carrier that isn't much longer than us: a carrier's own
        // U-turn child hears the old length relayed and must not eat itself.
        bool const feeder = carrierKnown && !insurance && !born && carrier.length >= 2 * T.L &&
                            ((endgame && carrier.length > T.L && T.units > 1) || hiveFeeder);
        feederMode = feeder;
        if (feeder) {
            // wall-aware distance field from the carrier's head
            carrierDist.assign(N, INF);
            std::vector<int> q{carrier.pos};
            carrierDist[carrier.pos] = 0;
            for (size_t qi = 0; qi < q.size(); qi++) {
                int c = q[qi];
                for (int d = 0; d < 4; d++) {
                    if (mem.blocked[c] & (1 << d)) continue;
                    int n = step(c, d);
                    if (carrierDist[n] != INF) continue;
                    carrierDist[n] = carrierDist[c] + 1;
                    q.push_back(n);
                }
            }
        }

        std::string why;
        bool acted = false;
        std::vector<int> moveDirs;

        // --- big enemy heads in view draw our small dragons in for a head-on
        huntVal.clear();
        if (!iAmCarrier && !feeder && T.units >= MIN_UNITS_TO_TRADE)
            for (size_t k = 0; k < T.enemyHeads.size(); k++) {
                int const le = T.enemyHeadLen[k];
                if (le < std::max(HUNT_MIN_LEN, T.L + 3)) continue;
                if (huntVal.empty()) huntVal.assign(N, 0.f);
                for (int d = 0; d < 4; d++) huntVal[step(T.enemyHeads[k], d)] = 12.f;
            }

        // Hunters: a few small dragons chase the enemy carrier's reported spot.
        // A head-on kills it whatever its length, which settles a length race.
        bool const intelFresh = enemyBig.pos >= 0 && T.round - enemyBig.round <= INTEL_TTL;
        bool const hunter = !iAmCarrier && !feeder && HUNTER_MOD > 0 && myId % HUNTER_MOD == 1 &&
                            T.round >= HUNT_FROM && T.L <= 5 && intelFresh && enemyBig.length >= HUNT_TARGET_LEN;
        if (hunter) {
            if (huntVal.empty()) huntVal.assign(N, 0.f);
            int const c = enemyBig.pos;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    int const n = idx(((px(c) + dx) % W + W) % W, ((py(c) + dy) % H + H) % H);
                    huntVal[n] = std::max(huntVal[n], 300.f / (1 + std::abs(dx) + std::abs(dy)));
                }
        }

        // --- evaluate the four single steps
        Eval ev[4];
        int const need = std::min(T.L + 2, 60);
        for (int d = 0; d < 4; d++) {
            int n = step(T.head, d);
            if (!enterable(T.head, d, n, 1)) continue;
            ev[d] = evaluate(n, need, true);
            ev[d].survive = survives(n, std::min(T.L + 2, 40), false);
            ev[d].strict = ev[d].survive && survives(n, std::min(T.L + 2, 40), true);
            for (int e : T.enemyHeads) if (e == n) ev[d].headOn = true;
            for (int a : T.allyHeads) if (a == n) ev[d].legal = false;
            if (ev[d].headOn) ev[d].legal = false; // handled by lunge logic
        }

        // --- 1. lunge into a bigger enemy head (never trade our way into elimination)
        // Guards also trade evenly with any enemy head that gets near our carrier.
        if (!iAmCarrier && !acted && T.units >= std::min(MIN_UNITS_TO_TRADE, 3)) {
            int bestGain = 0;
            std::vector<int> bestPath;
            for (size_t k = 0; k < T.enemyHeads.size(); k++) {
                int le = T.enemyHeadLen[k];
                int maxSteps = std::min(T.L - 1, 3);
                if (maxSteps < 1) continue;
                // Every dragon beacons itself until a real carrier exists, so only
                // guard one that is long enough to be worth an even trade.
                bool const guarding = carrierKnown && T.round >= CARRIER_FROM && carrier.length >= GUARD_MIN_LEN &&
                                      manhattan(T.enemyHeads[k], carrier.pos) <= GUARD_RADIUS;
                if (!guarding && T.units < MIN_UNITS_TO_TRADE) continue;
                std::vector<int> path;
                if (!findLunge(T.enemyHeads[k], maxSteps, path)) continue;
                int costLen = T.L; // we die too
                int gain = le - costLen;
                if (guarding) gain = std::max(gain, 1) + 100;
                bool const late = T.round > 440;
                if (gain > bestGain || (late && le >= 8 && gain >= 0 && bestPath.empty())) {
                    bestGain = gain;
                    bestPath = path;
                }
            }
            if (!bestPath.empty()) {
                moveDirs = bestPath;
                acted = true;
                why = "LUNGE";
            }
        }

        // --- 2. feeders sacrifice next to the carrier
        if (!acted && feeder) {
            int const roundsLeft = 500 - T.round;
            // Close to the carrier, it can see what we drop, it has little
            // food left in reach, and there is time to eat most of our pearls.
            int pearlsNearCarrier = 0;
            for (auto const& tile : ct.get_tiles())
                if (tile.has_pearl() && manhattan(idx(tile.position.x, tile.position.y), carrier.pos) <= 3)
                    pearlsNearCarrier++;
            if (manhattan(T.head, carrier.pos) <= FEED_RADIUS && T.inVision[carrier.pos] &&
                pearlsNearCarrier < (endgame ? FEED_MAX_PEARLS_END : FEED_MAX_PEARLS) && roundsLeft > 8) {
                why = "SACRIFICE";
                acted = true; // print no action: the engine counts it as death, body becomes pearls
            }
        }

        // --- 3. split
        if (!acted && !iAmCarrier && !(feeder && carrierD <= FEED_NEAR) && T.round < SPLIT_UNTIL &&
            T.L >= SPLIT_LEN && T.units < popCap &&
            ct.can_split(T.L / 2)) {
            int crowd = 0;
            for (auto const& tile : ct.get_tiles())
                if (auto const* p = tile.get_dragon(); p && p->get_team() == myTeam && p->get_id() != myId) crowd++;
            // parent needs a safe move next turn too
            bool anySafe = false;
            for (int d = 0; d < 4; d++)
                if (ev[d].safe(need)) anySafe = true;
            if (crowd <= SPLIT_CROWD && anySafe) {
                ct.do_split(T.L / 2);
                why = "SPLIT";
                acted = true;
                headHist.resize(std::min<int>(headHist.size(), T.L - T.L / 2));
            }
        }

        // --- 4. move
        if (!acted) {
            int best = -1;
            double bestScore = -1e18;
            bool const cautious = iAmCarrier || T.L >= CAUTIOUS_LEN || T.round > 470;
            for (int d = 0; d < 4; d++) {
                if (!ev[d].legal) continue;
                double s = 0;
                if (ev[d].safe(need)) s += ev[d].strict ? 2e7 : 1e7;
                else if (ev[d].survive) s += 5e6 + ev[d].space * 1e3;
                else s += ev[d].space * 1e3;
                // A head-on kills both. Stay out of reach when it would be a
                // good trade for them, or when we can't afford to lose anyone.
                if (iAmCarrier && T.threatFar[step(T.head, d)]) s -= 3e5;
                if (iAmCarrier && !T.enemyHeads.empty()) {
                    int nd = INF;
                    for (int e : T.enemyHeads) nd = std::min(nd, manhattan(e, step(T.head, d)));
                    s += std::min(nd, 7) * (double)FLEE_WEIGHT;
                }
                if (ev[d].threat) {
                    int const tl = T.threatLen[step(T.head, d)];
                    if (cautious) s -= 3e5;
                    else if (T.units <= LOW_UNITS) s -= THREAT_LOW;
                    else if (T.L > tl) s -= THREAT_BIGGER;
                    else s -= 2.0;
                }
                s += ev[d].value * 100.0;
                // feeders head to the carrier
                if (feeder) s -= carrierDist[step(T.head, d)] * 50.0;
                if (T.allyNear[step(T.head, d)]) s -= 1.0;
                s += std::uniform_real_distribution<double>(0, 0.01)(rng);
                if (s > bestScore) { bestScore = s; best = d; }
            }
            bool const trapped = best < 0 || !ev[best].safe(need);
            // Trapped but the next step eats: take the pearl first and U-turn
            // next turn (a U-turn needs no free tile). Dead-end farms pay this way.
            // Only when the child will have room: eating keeps the tail (the
            // child's birthplace) where it is, so a child boxed in now stays boxed
            // in, and a dragon spanning a corridor between two dead ends would
            // ping-pong forever instead of shrinking until it fits out.
            bool const eatFirst = trapped && best >= 0 && T.L >= 4 && T.inVision[step(T.head, best)] &&
                                  mem.pearl[step(T.head, best)] && T.units < ct.unit_limit - 1 &&
                                  tailRoom(std::min(T.L + 4, 60)) >= std::min(T.L + 2, 60);
            if (trapped && !eatFirst && T.L >= 4 && ct.can_split(T.L - 2) && T.L - 2 >= 2) {
                // Escape: the old tail leaves as a new dragon; this 2-cell stub stays.
                // Only worth it when the stub really is boxed in.
                bool const stubDoomed = best < 0 || ev[best].space < 3;
                if (stubDoomed || T.L >= 6) {
                    ct.do_split(T.L - 2);
                    why = "UTURN";
                    acted = true;
                    headHist.resize(std::min<int>(headHist.size(), 2));
                    if (iAmCarrier) {
                        // Hand over: beacon a length just under the child's, so the
                        // child (which hears it) takes over at once.
                        handedOver = true;
                        carrier = {myId, T.L - 3, T.head, T.round};
                    }
                }
            }
            if (!acted) {
                if (best < 0) {
                    // Nothing legal, so this dragon dies. Die usefully: ram an enemy
                    // head (a free trade), else try a portal (unknown exit, may
                    // live), and never take a teammate's head down with us.
                    int bestKind = -100;
                    for (int d = 0; d < 4; d++) {
                        if (mem.kelp[T.head] & (1 << d)) continue;
                        int const n = step(T.head, d);
                        int kind = 0;
                        if (std::find(T.enemyHeads.begin(), T.enemyHeads.end(), n) != T.enemyHeads.end()) kind = 3;
                        else if (std::find(T.allyHeads.begin(), T.allyHeads.end(), n) != T.allyHeads.end()) kind = -3;
                        else if (mem.blocked[T.head] & (1 << d)) kind = 1; // portal
                        if (kind > bestKind) { bestKind = kind; best = d; }
                    }
                    if (best < 0) best = 0;
                    why = "DOOMED";
                } else {
                    why = trapped ? "TIGHT" : "MOVE";
                }
                moveDirs = {best};
                acted = true;
            }
        }

        if (!moveDirs.empty()) {
            std::vector<Direction> ds;
            for (int d : moveDirs) ds.push_back(Direction(DC[d]));
            if (ds.size() == 1) ct.make_move(ds[0]);
            else ct.make_moves(ds);
            recordMove(moveDirs);
        }

        // --- sonar out: beacon relays the carrier
        if (why != "SACRIFICE") {
            Msg m{MSG_CARRIER, T.round & 511, carrier.id < 0 ? myId : carrier.id,
                  carrier.id < 0 ? T.L : carrier.length,
                  px(carrier.pos < 0 ? T.head : carrier.pos), py(carrier.pos < 0 ? T.head : carrier.pos)};
            m.round = (carrier.round < 0 ? T.round : carrier.round) & 511;
            uint64_t v = encode(m);
            uint64_t side = v;
            if ((T.round & 1) && enemyBig.pos >= 0 && T.round - enemyBig.round <= INTEL_TTL) {
                Msg e{MSG_ENEMY, enemyBig.round & 511, 0, enemyBig.length, px(enemyBig.pos), py(enemyBig.pos)};
                side = encode(e);
            }
            for (int d = 0; d < 4; d++) ct.send_sonar(Direction(DC[d]), (d & 1) ? side : v);
        }

        ct.set_indicator_string(why + (iAmCarrier ? " C" : ""));
        if (debugOn && iAmCarrier && T.round % 10 == 0)
            ct.output_log("CARR", T.round, "id", myId, "L", T.L, "pearlsInView",
                          (int)std::count_if(ct.get_tiles().begin(), ct.get_tiles().end(), [](Tile const& t) { return t.has_pearl(); }));
        if (debugOn && why == "SACRIFICE") ct.output_log("SAC", T.round, "L", T.L, "cd", manhattan(T.head, carrier.pos));
        if (debugOn && (T.round == 100 || T.round == 300 || T.round == 420 || T.round == 470))
            ct.output_log("CARRIER", T.round, "me", myId, "L", T.L, "carrier", carrier.id, "clen", carrier.length,
                          "age", T.round - carrier.round, "dist", carrier.pos >= 0 ? manhattan(T.head, carrier.pos) : -1,
                          iAmCarrier ? "SELF" : "");
        if (debugOn && iAmCarrier) {
            std::string en;
            for (size_t k = 0; k < T.enemyHeads.size(); k++)
                en += std::to_string(px(T.enemyHeads[k])) + "," + std::to_string(py(T.enemyHeads[k])) + "/" +
                      std::to_string(T.enemyHeadLen[k]) + " ";
            std::string dirs;
            for (int d = 0; d < 4; d++)
                dirs += std::string(1, DC[d]) + (ev[d].legal ? "" : "x") + (T.threatFar[step(T.head, d)] ? "!" : "") +
                        (ev[d].safe(need) ? "s" : "") + " ";
            ct.output_log("CDBG head", px(T.head), py(T.head), "enemies", en, "dirs", dirs);
        }
        if (debugOn && (why == "DOOMED" || why == "TIGHT" || why == "UTURN")) {
            std::string b;
            for (int i : T.body) b += std::to_string(px(i)) + "," + std::to_string(py(i)) + " ";
            ct.output_log("L", T.L, "head", px(T.head), py(T.head), "body", b);
            for (int d = 0; d < 4; d++)
                ct.output_log(DC[d], "legal", ev[d].legal, "space", ev[d].space, "surv", ev[d].survive,
                              "blk", (int)mem.blocked[T.head], "free", T.freeAt[step(T.head, d)]);
        }
        born = false;
        end_turn();
    }
    (void)born;
}
