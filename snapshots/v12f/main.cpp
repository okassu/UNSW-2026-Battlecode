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
int AREA_PER_DRAGON = param("AREA_PER_DRAGON", 7);       // population cap = reachable area / this (top teams: ~7)
int SPLIT_CROWD = param("SPLIT_CROWD", 40);              // no split with more ally segments than this in view
int SPLIT_ROOM = param("SPLIT_ROOM", 3);                 // split only if the child (born on our tail) can reach this many free
                                                         // tiles: in a 2x2 farm loop it was born boxed in and died at once,
                                                         // 2 cells for 1 pearl (190 of 1226 splits on slithery; 0 = off)
int SMALL_LEN = param("SMALL_LEN", 3);                   // dragons this short are disposable: food first, safety second
                                                         // (v9: 3 v 0, MAPLIB=0: v v8 0.660 v 0.570, v aggressive swarm 0.560 v 0.530)
int DISPOSE_MODE = param("DISPOSE_MODE", 2);             // 1: any pearl in view beats safety; 2: only the pearl one step ahead
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
int PORTALS = param("PORTALS", 1);                       // cross portals whose exit is known (library, or learned in play)
int MAPLIB = param("MAPLIB", 1);                         // 0: ignore the map library (tests play every map as unknown)
int SCOUT_LEN = param("SCOUT_LEN", 4);                   // dragons this short may cross a portal whose exit is unknown
int SCOUT_VALUE = param("SCOUT_VALUE", 30);              // target value (x10) of crossing an unknown portal
int PORTAL_SHARE = param("PORTAL_SHARE", 1);             // relay learned portal pairs on sonar
int SCOUT_FRONTIER = param("SCOUT_FRONTIER", 8);         // scout only when fewer unseen tiles than this are walkable
int CARRIER_PORTALS = param("CARRIER_PORTALS", 0);       // 0: the carrier treats portals as walls
int PORTAL_COST = param("PORTAL_COST", 8);               // route choice: a portal crossing counts as this many extra steps
int CLAIM_MIN_LEN = param("CLAIM_MIN_LEN", 12);          // before CLAIM_ANY_FROM only the king or a dragon this long becomes carrier
int CLAIM_ANY_FROM = param("CLAIM_ANY_FROM", 200);       // ...from this round any group's longest dragon may (330 until v12)
int STALL_WIN = param("STALL_WIN", 8);                   // moves circling <= 4 tiles without growing = stalled (0 = off)
int GIVEUP_ROUNDS = param("GIVEUP_ROUNDS", 40);          // a stalled dragon ignores the targets it chased for this long
int REGION_W = param("REGION_W", 50);                    // x100: weight of the distance-discounted value in a direction
int REGION_GAMMA = param("REGION_GAMMA", 85);            // x100: discount per step for that sum
int REGION_PEARLS = param("REGION_PEARLS", 1);           // 1: that sum leaves out the curiosity bonus of never-seen tiles
int REGION_AVG = param("REGION_AVG", 1);                 // 1: use the discount-weighted average over the tiles reached, not the sum
int GIVEUP_ADJ = param("GIVEUP_ADJ", 40);                // rounds to ignore a target we are next to but won't step onto
int KEEP_UNITS = param("KEEP_UNITS", 3);                 // endgame feeding stops at this many dragons (they keep eating)
int FARM_ENTRY = param("FARM_ENTRY", 1);                 // enter a dead-end pocket whose pearls make us long enough to split/U-turn out
int POCKET_MAX = param("POCKET_MAX", 16);                // ...a pocket is at most this many tiles behind its mouth
int FARM_MIN = param("FARM_MIN", 3);                     // ...and holds at least this many pearls: leaving costs a 2-cell stub, so
                                                         // k pearls gain k - 2 (ladder autarky: 49 of 53 early self-hits were such
                                                         // stubs after 2-pearl visits; we out-ate them 207 v 90 by r150, then had
                                                         // 8 dragons to their 29 at r200)
int STRICT_BONUS = param("STRICT_BONUS", 10000000);      // score for a safe step no other head can cut off
int TRADE_EVEN = param("TRADE_EVEN", 0);                 // with this many dragons, small ones trade evenly where we outnumber (0 = off)
// v9: trades follow the local head count. A head-on's drops go to whoever has more mouths
// nearby (ladder: outnumbered, they ate 901 v our 336 within 3 tiles of the trade; ahead, we
// ate 1072 v 249), so trade evenly where we're ahead and keep out of reach where we're behind.
int TRADE_MODE = param("TRADE_MODE", 1);                 // 1: ahead/even/behind rule below; 0: v8
int AHEAD_MARGIN = param("AHEAD_MARGIN", 1);             // our heads in view (incl. us) - theirs >= this: ahead
                                                         // (2 was the plan; with SMALL_LEN=3 and CONTEST_W=100, 1: v v8 0.760, 2: 0.720)
int TRADE_LEN = param("TRADE_LEN", 3);                   // ahead, dragons this short trade evenly
int TRADE_DOWN = param("TRADE_DOWN", 0);                 // well ahead, a small dragon rams an enemy up to this much shorter: top
int TRADE_DOWN_MARGIN = param("TRADE_DOWN_MARGIN", 2);   // winners started 24% of their head-ons longer than the victim, 4:1
int TRADE_DOWN_LEN = param("TRADE_DOWN_LEN", 5);         // ahead:behind (their count is what gets eliminated; 0 = off)
int SEEN_TTL = param("SEEN_TTL", 2);                     // enemy heads seen this many rounds ago still count...
int SEEN_RADIUS = param("SEEN_RADIUS", 5);               // ...if they were this close (they may be just out of view)
int BEHIND_FLEE = param("BEHIND_FLEE", 0);               // behind: score per tile of distance from the nearest enemy head
                                                         // (300 made us passive: v8 0.470 / -4.0 longest; 0: 0.560 / -0.4)
int SYMMETRY = param("SYMMETRY", 1);                     // infer the map's symmetry and mirror what we see (0 = off)
int SYM_CONFIRM = param("SYM_CONFIRM", 20);              // matched wall/portal tiles before a map passing all three counts as fully symmetric
int LOOP_FAST = param("LOOP_FAST", 1);                   // stall check after one full circle (L+2 moves), not STALL_WIN
int LOOP_WAIT = param("LOOP_WAIT", 3);                   // ...unless a chased tile spawns within this many rounds
int LOOP_TABU = param("LOOP_TABU", 60);                  // score cost of stepping onto one of our last 12 head tiles...
int LOOP_TABU_STALL = param("LOOP_TABU_STALL", 400);     // ...and for 10 rounds after a stall (giving targets up alone left
                                                         // dragons circling: the next-best target sits right beside)
// v9b: map control on unknown maps. Contested ground (tiles near their own mirror image are as
// far from one side's spawns as from the other's) starts out worth what a field is; tiles known
// only by their mirror or by sonar are valued from their spawn rate (raid targets included).
int CONTEST_W = param("CONTEST_W", 100);                 // x100: never-seen tile value boost at the contested line
                                                         // (SMALL_LEN=3, MAPLIB=0, v v8, 2 seeds: 200 0.650, 100 0.720, 0 0.670)
int CONTEST_RANGE = param("CONTEST_RANGE", 0);           // tiles from the line where the boost fades out (0 = (W+H)/8)
int CONTEST_HOME = param("CONTEST_HOME", 0);             // no boost within this many steps of our birth tile, full at twice
                                                         // that: spawns touching across the wrap seam make the whole seam
                                                         // "contested" (0 = off)
int RATE_VALUE = param("RATE_VALUE", 1);                 // value never-seen tiles whose rate is known (mirror/sonar)
int RATE_DISC = param("RATE_DISC", 60);                  // x100: ...discounted, since someone may be eating there
int SOURCE_SHARE = param("SOURCE_SHARE", 1);             // relay fast-spawning tiles on sonar
int SOURCE_GAP = param("SOURCE_GAP", 40);                // ...those whose countdown never exceeded this
int SCOUT_MOD = param("SCOUT_MOD", 8);                   // 1 in this many small dragons scouts: vision first, may cross unknown portals (0 = off)
// v9c: one real carrier, grown in time. On the ladder, from round 330 (CLAIM_ANY_FROM) 13-28 of
// our 2-4 cell dragons believed they were the carrier at once (a dragon hearing no beacon beacons
// itself and claims on the echo), fed by 1-2 pearl sacrifices; at r350 our longest was 4-10 v
// their 10-20, and we had split everything down to 2+2, so nobody was long enough to carry.
int GROW_FROM = param("GROW_FROM", 280);                 // from this round growers stop splitting...
int GROWER_MOD = param("GROWER_MOD", 5);                 // ...1 in this many dragons (0 = off)
int CLAIM_LATE_LEN = param("CLAIM_LATE_LEN", 6);         // after CLAIM_ANY_FROM a claimant must be at least this long
int QUIET_SMALL = param("QUIET_SMALL", 1);               // a dragon too short to be carrier doesn't beacon itself
int LATE_EVEN_MAXLEN = param("LATE_EVEN_MAXLEN", 6);     // late even trades into big enemies only by dragons this short
// v10 (ladder batch 2026-09-28 evening, 4 W 6 L): portals decided 5 of the 6 losses.
int PORTAL_RISK = param("PORTAL_RISK", 1);               // 1: a known crossing costs by risk, not a flat PORTAL_COST
int PORTAL_CHEAP = param("PORTAL_CHEAP", 1);             // ...extra steps when the exit is probably empty (early, or seen empty lately)
int PORTAL_MID = param("PORTAL_MID", 4);                 // ...when we know nothing recent about the exit
int PORTAL_EARLY = param("PORTAL_EARLY", 60);            // exits count as empty before this round...
int PORTAL_EARLY_SIDE = param("PORTAL_EARLY_SIDE", 2);   // ...only if at least this much closer to our birth tile than to its
                                                         // mirror (0 = any exit). v10 treated every early exit as empty and sent
                                                         // two dragons through trophy's shortcut, on the symmetry line, into
                                                         // devtest1's swarm (r29-47); locally early crossing still paid on portal maps.
int PORTAL_FRESH = param("PORTAL_FRESH", 6);             // exit seen within this many rounds with no enemy near: cheap
int PORTAL_DANGER = param("PORTAL_DANGER", 10);          // enemy head seen within 2 of the exit this recently: PORTAL_COST
int RALLY = param("RALLY", 1);                           // report rich ground the enemy holds; small dragons gather there
int RALLY_TTL = param("RALLY_TTL", 25);                  // rounds a rally report stays useful
int RALLY_RANGE = param("RALLY_RANGE", 20);              // dragons this close (manhattan) answer it
int RALLY_VALUE = param("RALLY_VALUE", 40);              // x10: target value around the rally spot
int RALLY_MIN_ENEMY = param("RALLY_MIN_ENEMY", 2);       // enemy heads near a rich tile that make it a rally spot
int RALLY_FROM = param("RALLY_FROM", 60);                // no rally reports before this round (opening contact is normal:
                                                         // from round 0 the weak-map set fell to 0.143 v v9; from 60 0.429)
int PRIZE = param("PRIZE", 1);                           // rich tiles seen but unreachable on foot draw small dragons to unknown portals
int PRIZE_RADIUS = param("PRIZE_RADIUS", 10);            // unknown portal mouths this close to a prize tile
int PRIZE_VALUE = param("PRIZE_VALUE", 60);              // x10: value of such a mouth
int PRIZE_TTL = param("PRIZE_TTL", 80);                  // rounds a prize sighting is remembered
int BORN_UTURN = param("BORN_UTURN", 0);                 // 0: a newborn doesn't U-turn on its first turn (16 -> 8 in one round on schooltime)
int BIG_UTURN_LEN = param("BIG_UTURN_LEN", 10);          // carriers and dragons this long U-turn only with no survivable move
int ESCORT_FROM = param("ESCORT_FROM", 400);             // from this round some small dragons escort the carrier
int ESCORT_MOD = param("ESCORT_MOD", 4);                 // ...1 in this many (0 = off)
int ESCORT_RADIUS = param("ESCORT_RADIUS", 8);           // ...if this close to it
int CLAIM_GATE = param("CLAIM_GATE", 1);                 // hearing no carrier at all, claim only under the same rules (v8 let any 5+ claim;
                                                         // harmless while all split to 2+2, 18-39 carriers per 50 rounds with growers)
// v11: the carrier also dodges enemy heads it saw in the last SEEN_TTL rounds and no longer sees, within
// 3 + age steps: one just outside view can sprint into us after we step toward it (v10's 22 and 20 cell
// carriers died head-on on plain moves on slithery_fight, r415 and r432).
int CARRIER_MEMTHREAT = param("CARRIER_MEMTHREAT", 200000);
// v11: grow the carrier early by feeding it, not by sacrifices: from CARRIER_FOOD_FROM, teammates leave pearls
// within CARRIER_FOOD_RADIUS of the carrier (three ladder teams had a 15-28 cell dragon at r200 and beat us on
// length while we out-ate them; our longest was 5-6 until r350).
int CARRIER_FOOD_FROM = param("CARRIER_FOOD_FROM", 150); // 0 = off (tuning: 150 with claims from 200 gave +1.81 longest, the best)
int CARRIER_FOOD_RADIUS = param("CARRIER_FOOD_RADIUS", 3);
// v12: a head-on kills any length, so for the carrier and dragons this long ending a move in an enemy's reach
// costs more than the safety tiers (1e7 safe, +1e7 strict). At 3e5 our carriers stepped next to 2-3 cell enemy
// heads whenever the alternative had a little less room: 27 and 19 cells lost that way on slithery_fight.
int BIG_THREAT_LEN = param("BIG_THREAT_LEN", 12);
int BIG_THREAT = param("BIG_THREAT", 25000000);
// v12: outnumbered locally, ending in an enemy's reach costs more than a safe move is worth over a merely
// survivable one (1e7 v 5e6), so the dodge wins; at 3e5 it lost to a little more room every time (ladder: 12-68
// trades per game started by them while they had more heads around, 68 on schooltime).
int BEHIND_THREAT = param("BEHIND_THREAT", 8000000);
// v12: the opening race. Our first dragons took the nearest pearls (value / (steps + 1)) while the teams beating
// us went straight for the rich contested ground: devil's centre field at r11 v our r30 (15 v 0 eats by r24),
// trophy's field, schooltime's portal field. For OPENING_ROUNDS a target's value is divided by sqrt(steps + 1)
// and every tile (not only never-seen ones) gets the contested-line boost.
int OPENING_ROUNDS = param("OPENING_ROUNDS", 0);         // 0 = off
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
    std::vector<uint8_t> blocked;   // bit d: can't cross the edge (kelp, or a portal with no known exit)
    std::vector<uint8_t> kelp;      // bit d: kelp specifically
    std::vector<int> exitTo;        // [i*4+d]: tile a crossing of portal edge d of i lands on, -1 not a known portal
    std::vector<uint8_t> portal;    // bit d: the edge is a portal (exit known or not)
    std::vector<int> lastSeen;      // round the tile was last in vision, -1 never
    std::vector<int> spawnAt;       // absolute round of next spawn attempt, -1 never spawns
    std::vector<uint8_t> pearl;     // pearl present when last seen
    std::vector<float> exploreBias; // per-dragon random preference for regions
    std::vector<int> gapLo, gapHi;  // spawn gap range per tile (from the map library), -1 unknown
    std::vector<int> cdMax;         // longest pearl countdown seen on the tile: a rough gap estimate, -1 none
    std::vector<uint8_t> cdSeen;    // rounds the countdown was seen (capped), for trusting cdMax
    std::vector<uint8_t> mirrored;  // tile known only through the map's symmetry, never seen
    std::vector<int> enemyNear;     // last round an enemy head was seen within 2 of the tile, -1 never
    std::vector<int> prizeAt;       // last round the tile was seen rich but out of reach on foot, -1 never
    void init() {
        gapLo.assign(N, -1);
        gapHi.assign(N, -1);
        cdMax.assign(N, -1);
        cdSeen.assign(N, 0);
        mirrored.assign(N, 0);
        enemyNear.assign(N, -1);
        prizeAt.assign(N, -1);
        blocked.assign(N, 0);
        kelp.assign(N, 0);
        exitTo.assign(4 * N, -1);
        portal.assign(N, 0);
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

// The tile a step across edge d of tile i lands on: the neighbour, or a portal's exit.
inline int go(int i, int d) {
    int const e = mem.exitTo[i * 4 + d];
    return e >= 0 ? e : step(i, d);
}
inline bool unknownPortal(int i, int d) { return (mem.portal[i] & (1 << d)) && mem.exitTo[i * 4 + d] < 0; }

// ---------------------------------------------------------------- portals
// An edge named once: tile*4+dir, from the tile south of it (N) or east of it (W).
inline int canonEdge(int i, int d) {
    if (d == 2) return step(i, 2) * 4 + 0;
    if (d == 1) return step(i, 1) * 4 + 3;
    return i * 4 + d;
}

// Open (or close) the portal pair ea-eb. A crossing keeps its direction: in through
// one edge, out of the partner edge on its far side (all 22 kinds of crossing in a
// trauma ladder game, and every crossing BitShift made on the "portals" map).
void linkPortal(int ea, int eb, bool open) {
    if (ea % 4 != eb % 4) return; // crossed orientations: exit rule unverified, stays a wall
    int const d = ea % 4, od = (d + 2) & 3;
    for (auto [t, u] : {std::pair{ea / 4, eb / 4}, std::pair{eb / 4, ea / 4}}) {
        int const far = step(t, d); // the edge's other side
        mem.exitTo[t * 4 + d] = open ? step(u, d) : -1;
        mem.exitTo[far * 4 + od] = open ? u : -1;
        mem.portal[t] |= 1 << d;
        mem.portal[far] |= 1 << od;
        if (open) {
            mem.blocked[t] &= ~(1 << d);
            mem.blocked[far] &= ~(1 << od);
        } else {
            mem.blocked[t] |= 1 << d;
            mem.blocked[far] |= 1 << od;
        }
    }
}

// Portal edges seen in play, by id. Each id marks exactly two edges on every
// library map, so the second sighting (in vision, just after a blind crossing, or
// relayed by sonar) gives both exits. A third edge means that assumption is wrong.
std::unordered_map<int, std::vector<int>> portalSeen;
int mapKnown = -1; // library index once identified
int symKnown = -1; // the map's symmetry once worked out (see checkSymmetry), -1 unknown
int birthTile = -1; // our head on our first turn

void notePortal(int id, int e) {
    if (!PORTALS || id < 0) return;
    auto& v = portalSeen[id];
    if (std::find(v.begin(), v.end(), e) != v.end()) return;
    v.push_back(e);
    if (mapKnown >= 0) return; // the library already linked it
    if (v.size() == 2) linkPortal(v[0], v[1], true);
    if (v.size() == 3) linkPortal(v[0], v[1], false);
}
void relinkLearned() {
    for (auto const& [id, v] : portalSeen)
        if (v.size() == 2) linkPortal(v[0], v[1], true);
}

std::vector<int> headHist; // head positions, newest first, one per step taken
std::unordered_map<int, std::pair<int, int>> enemySeen; // enemy id -> (head, round) last seen
// Stall detection: a dragon circling a few tiles without eating is chasing something it
// can't get (a pearl in a dead-end notch it may not enter, a spawn tile under its own
// body). 6-14% of our moves on the ladder were such loops. It gives those targets up.
std::deque<std::pair<int, int>> trail; // (head, target) per move, newest last
std::deque<int> trailLen;              // length at each of those moves
std::unordered_map<int, int> giveUp;   // tile -> round until which it is ignored
std::deque<int> recentHeads;           // our last 12 head tiles, for the loop penalty
int stallUntil = -1;                   // round until which the loop penalty is raised
std::vector<int> histBeforeMove, lastDirs; // last turn's move, to replay it once a blind crossing's exit is known
int lastHead = -1;

// ---------------------------------------------------------------- sonar
// 64-bit message: [tag 12][type 2][round 9][id 13][length 8][sym 2][x 6][y 6][check 6]
// sym: 0 unknown, else 1 + the map symmetry the sender worked out (a newborn knows nothing).
constexpr uint64_t TAG = 0xB7D;
enum MsgType { MSG_SOURCE = 0, MSG_CARRIER = 1, MSG_ENEMY = 2, MSG_PORTAL = 3 };
struct Msg {
    int type, round, id, length, x, y;
    int sym = 0;
};
uint64_t teamSalt() { return myTeam == Team::A ? 0x5A17C0DE31ULL : 0x2B0BCAFE77ULL; }
uint64_t encode(Msg const& m) {
    uint64_t v = TAG;
    v = (v << 2) | (m.type & 3);
    v = (v << 9) | (m.round & 511);
    v = (v << 13) | (m.id & 8191);
    v = (v << 8) | std::min(m.length, 255);
    v = (v << 2) | (m.sym & 3);
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
    m.sym = b & 3; b >>= 2;
    m.length = b & 255; b >>= 8;
    m.id = b & 8191; b >>= 13;
    m.round = b & 511; b >>= 9;
    m.type = b & 3;
    return true;
}
// A portal pair in the same frame: [id 12][edge a 13][edge b 13][spare 4][sym 2] after
// the type; an edge is [x 6][y 6][W?1] of its canonical tile.
inline uint64_t packEdge(int e) { return (uint64_t)px(e / 4) << 7 | (uint64_t)py(e / 4) << 1 | (e % 4 == 3); }
inline int unpackEdge(uint64_t p) {
    int const x = (p >> 7) & 63, y = (p >> 1) & 63;
    return x < W && y < H ? idx(x, y) * 4 + ((p & 1) ? 3 : 0) : -1;
}
uint64_t encodePortal(int id, int ea, int eb, int sym) {
    uint64_t v = TAG;
    v = (v << 2) | MSG_PORTAL;
    v = (v << 12) | (id & 4095);
    v = (v << 13) | packEdge(ea);
    v = (v << 13) | packEdge(eb);
    v = (v << 6) | (sym & 3);
    v <<= 6;
    uint64_t h = ((v >> 6) ^ teamSalt()) * 0x9E3779B97F4A7C15ULL;
    return v | ((h >> 58) & 63);
}
// For a MSG_PORTAL frame that decode() accepted.
void decodePortal(uint64_t v, int& id, int& ea, int& eb) {
    uint64_t b = v >> 12;
    eb = unpackEdge(b & 8191); b >>= 13;
    ea = unpackEdge(b & 8191); b >>= 13;
    id = b & 4095;
}

// Two fast-spawning tiles in one frame: [x 6][y 6][gap 5] twice, then [sym 2][spare 8].
// gap 0 marks an empty slot.
uint64_t encodeSources(int a, int ga, int b, int gb, int sym) {
    uint64_t v = TAG;
    v = (v << 2) | MSG_SOURCE;
    for (auto [t, g] : {std::pair{a, ga}, std::pair{b, gb}}) {
        bool const ok = t >= 0 && g > 0;
        v = (v << 6) | (ok ? px(t) : 0);
        v = (v << 6) | (ok ? py(t) : 0);
        v = (v << 5) | (ok ? std::min(g, 31) : 0);
    }
    v = (v << 2) | (sym & 3);
    v <<= 8;
    v <<= 6;
    uint64_t h = ((v >> 6) ^ teamSalt()) * 0x9E3779B97F4A7C15ULL;
    return v | ((h >> 58) & 63);
}
// For a MSG_SOURCE frame that decode() accepted: up to two (tile, gap) pairs, and sym.
int decodeSources(uint64_t v, int* tiles, int* gaps) {
    uint64_t b = v >> 14;
    int const sym = b & 3;
    b >>= 2;
    for (int k = 1; k >= 0; k--) {
        int const g = b & 31, y = (b >> 5) & 63, x = (b >> 11) & 63;
        b >>= 17;
        tiles[k] = g > 0 && x < W && y < H ? idx(x, y) : -1;
        gaps[k] = g;
    }
    return sym;
}

struct Carrier {
    int id = -1, length = 0, pos = -1, round = -1000;
} carrier; // best-known team carrier (longest dragon heard of)
struct EnemyIntel {
    int length = 0, pos = -1, round = -1000;
} enemyBig; // biggest enemy heard of
// Rich ground the enemy holds (MSG_ENEMY with id 1): small dragons gather there, and the
// trade rule makes the push, holding them out of reach until ours outnumber theirs.
struct Rally {
    int pos = -1, round = -1000, enemies = 0;
} rally;

// ---------------------------------------------------------------- map library
std::vector<int> mapCands; // library maps still consistent with everything seen
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
    if (!MAPLIB) return;
    for (int k = 0; k < maplib::mapCount; k++)
        if (maplib::maps[k].w == W && maplib::maps[k].h == H) mapCands.push_back(k);
}

void applyMap(int k) {
    auto const& m = maplib::maps[k];
    std::fill(mem.exitTo.begin(), mem.exitTo.end(), -1);
    for (int i = 0; i < N; i++) {
        mem.kelp[i] = libKelp(m, i);
        mem.portal[i] = libPortal(m, i);
        mem.blocked[i] = libKelp(m, i) | libPortal(m, i);
        auto const& g = m.palette[libGapIndex(m, i)];
        mem.gapLo[i] = g.lo;
        mem.gapHi[i] = g.hi;
        if (g.hi == 0) mem.spawnAt[i] = -1;
    }
    for (int a = 0; PORTALS && a < m.portalCount; a++)
        for (int b = a + 1; b < m.portalCount; b++)
            if (m.portals[a].id >= 0 && m.portals[a].id == m.portals[b].id)
                linkPortal(canonEdge(m.portals[a].tile, m.portals[a].dir), canonEdge(m.portals[b].tile, m.portals[b].dir), true);
    mapKnown = k;
}

void symFromLibrary();

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
        // Wrong map after all: forget library walls (and the symmetry read off them), relearn from vision.
        mapKnown = -1;
        symKnown = -1;
        std::fill(mem.blocked.begin(), mem.blocked.end(), 0);
        std::fill(mem.kelp.begin(), mem.kelp.end(), 0);
        std::fill(mem.exitTo.begin(), mem.exitTo.end(), -1);
        std::fill(mem.portal.begin(), mem.portal.end(), 0);
        std::fill(mem.gapLo.begin(), mem.gapLo.end(), -1);
        std::fill(mem.gapHi.begin(), mem.gapHi.end(), -1);
        relinkLearned();
    }
    mapCands = keep;
    if (mapKnown < 0 && mapCands.size() == 1) {
        applyMap(mapCands[0]);
        symFromLibrary();
        if (debugOn) ct.output_log("MAPID", maplib::maps[mapKnown].name);
    }
}

// Tiles reachable from `from` (through portals with a known exit), using walls known so far.
int reachableArea(int from) {
    std::vector<uint8_t> seen(N, 0);
    std::vector<int> q{from};
    seen[from] = 1;
    for (size_t qi = 0; qi < q.size(); qi++)
        for (int d = 0; d < 4; d++) {
            if (mem.blocked[q[qi]] & (1 << d)) continue;
            int n = go(q[qi], d);
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
    std::vector<int> enemyHeadId;
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
            int n = go(cur, d);
            if (ownFace[n] >= 0 && !used[n] && go(n, ownFace[n]) == cur) {
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
    T.enemyHeadId.clear();
    std::vector<int> segCount(0);
    std::unordered_map<int, int> countById;

    for (auto const& tile : ct.get_tiles()) {
        int const i = idx(tile.position.x, tile.position.y);
        T.inVision[i] = 1;
        mem.lastSeen[i] = T.round;
        mem.pearl[i] = tile.has_pearl();
        int const pt = tile.get_pearl_time();
        mem.spawnAt[i] = pt < 0 ? -1 : T.round + pt;
        mem.mirrored[i] = 0;
        if (pt >= 0) {
            mem.cdMax[i] = std::max(mem.cdMax[i], pt);
            if (mem.cdSeen[i] < 255) mem.cdSeen[i]++;
        }
        for (int d = 0; d < 4; d++) {
            auto const& e = tile.edges[d];
            uint8_t const bit = 1 << d;
            if (e.edge_type == EdgeType::KELP) {
                mem.kelp[i] |= bit;
                mem.blocked[i] |= bit;
            } else if (e.edge_type == EdgeType::PORTAL) {
                notePortal(e.get_portal_id(), canonEdge(i, d));
                mem.portal[i] |= bit;
                if (mem.exitTo[i * 4 + d] < 0) mem.blocked[i] |= bit;
                else mem.blocked[i] &= ~bit;
                mem.kelp[i] &= ~bit;
            } else {
                mem.blocked[i] &= ~bit;
                mem.kelp[i] &= ~bit;
                mem.portal[i] &= ~bit;
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
            if (mem.portal[i] & (1 << d)) mem.portal[n] |= 1 << od;
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
                T.enemyHeadId.push_back(p->get_id());
                T.enemyHeadLen.push_back(countById[p->get_id()]);
                T.enemyReachLen.push_back(rimById.count(p->get_id()) ? std::max(RIM_LEN, countById[p->get_id()])
                                                                     : countById[p->get_id()]);
            }
        }
    }

    // Own body: facing chain from vision, extended with remembered head positions.
    std::vector<int> chain = ownChain(ct);
    // After a blind portal crossing the partner edge is in view now, so replay the
    // move through the exit it revealed: that keeps the body trail behind us.
    if (!headHist.empty() && headHist[0] != T.head && lastHead >= 0) {
        std::vector<int> h = histBeforeMove;
        int cur = lastHead;
        for (int d : lastDirs) {
            cur = go(cur, d);
            h.insert(h.begin(), cur);
        }
        if (cur == T.head) headHist = h;
    }
    lastHead = -1;
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
                int n = go(c, d);
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
                int n = go(c, d);
                if (T.headDist[n] != INF || T.freeAt[n] >= INF) continue;
                T.headDist[n] = T.headDist[c] + 1;
                q.push_back(n);
            }
        }
    }
    T.allyNear.assign(N, 0);
    for (int a : T.allyHeads)
        for (int d = 0; d < 4; d++) T.allyNear[go(a, d)] = 1;
}

// ---------------------------------------------------------------- symmetry
void noteSource(int i, int g);
// Every map is symmetric (the engine refuses a map whose mirror doesn't match): rot180
// ("xy"), left-right ("y") or top-bottom ("x"). Each seen tile whose image we have seen
// too rules out the symmetries it contradicts; once one is left, our side tells us theirs:
// walls, portals (pairs included), tiles that never spawn, and how slowly tiles spawn.
enum { SYM_ROT, SYM_MX, SYM_MY };
bool symAlive[3] = {true, true, true};
int symCount[3] = {0, 0, 0};           // matched tile pairs with a wall or portal
std::vector<uint8_t> symMatched[3];

inline int symImg(int s, int i) {
    int const x = px(i), y = py(i);
    if (s == SYM_ROT) return idx(W - 1 - x, H - 1 - y);
    if (s == SYM_MX) return idx(W - 1 - x, y);
    return idx(x, H - 1 - y);
}
inline int symDir(int s, int d) {
    if (s == SYM_ROT) return (d + 2) & 3;
    if (s == SYM_MX) return d == 1 ? 3 : d == 3 ? 1 : d;
    return d == 0 ? 2 : d == 2 ? 0 : d;
}
inline int symEdge(int s, int e) { return canonEdge(symImg(s, e / 4), symDir(s, e % 4)); }

// Copy what we saw of tile i onto its image, unless we have seen the image itself.
void mirrorTile(int i) {
    int const j = symImg(symKnown, i);
    if (j == i || mem.lastSeen[j] >= 0 || mem.lastSeen[i] < 0) return;
    mem.mirrored[j] = 1;
    for (int d = 0; d < 4; d++) {
        int const dj = symDir(symKnown, d), nj = step(j, dj), od = (dj + 2) & 3;
        uint8_t const b = 1 << dj, ob = 1 << od;
        if (mem.kelp[i] & (1 << d)) {
            mem.kelp[j] |= b, mem.blocked[j] |= b;
            mem.kelp[nj] |= ob, mem.blocked[nj] |= ob;
        } else if (mem.portal[i] & (1 << d)) {
            mem.portal[j] |= b, mem.portal[nj] |= ob;
            if (mem.exitTo[j * 4 + dj] < 0) mem.blocked[j] |= b;
            if (mem.exitTo[nj * 4 + od] < 0) mem.blocked[nj] |= ob;
        }
    }
    if (mem.spawnAt[i] == -1 && mem.spawnAt[j] == -2) mem.spawnAt[j] = -1;
    mem.cdMax[j] = std::max(mem.cdMax[j], mem.cdMax[i]);
    mem.cdSeen[j] = std::max(mem.cdSeen[j], mem.cdSeen[i]);
}

// A known portal pair's image is a pair too: its exits are known without crossing.
void mirrorPortals() {
    for (auto const& [id, v] : portalSeen)
        if (v.size() == 2) {
            int const a = symEdge(symKnown, v[0]), b = symEdge(symKnown, v[1]);
            if (mem.exitTo[a] < 0) linkPortal(a, b, true);
        }
}

void mirrorAll() {
    for (int i = 0; i < N; i++)
        if (mem.lastSeen[i] >= 0) mirrorTile(i);
    mirrorPortals();
}

// A teammate worked the symmetry out (sonar): take it unless our own sightings rule it out.
void adoptSymmetry(int s) {
    if (!SYMMETRY || symKnown >= 0 || mapKnown >= 0 || s < 0 || s > 2 || !symAlive[s]) return;
    symKnown = s;
    mirrorAll();
}

// A library map is known whole: its symmetry is the first candidate that maps every wall and portal
// onto a wall and portal (the side rule for early crossings needs it; checkSymmetry skips library maps).
void symFromLibrary() {
    for (int s = 0; s < 3 && symKnown < 0; s++) {
        bool ok = true;
        for (int i = 0; i < N && ok; i++) {
            int const j = symImg(s, i);
            for (int d = 0; d < 4 && ok; d++) {
                int const dj = symDir(s, d);
                if (((mem.kelp[i] >> d) & 1) != ((mem.kelp[j] >> dj) & 1)) ok = false;
                if (((mem.portal[i] >> d) & 1) != ((mem.portal[j] >> dj) & 1)) ok = false;
            }
        }
        if (ok) symKnown = s;
    }
}

// A teammate saw tile i spawn at least every g rounds; we haven't seen it ourselves.
// Its mirror image is the other side's matching source.
void noteSource(int i, int g) {
    if (!SOURCE_SHARE || g <= 0) return;
    for (int j : {i, symKnown >= 0 ? symImg(symKnown, i) : -1}) {
        if (j < 0 || mem.lastSeen[j] >= 0 || mem.cdMax[j] >= 0) continue;
        mem.cdMax[j] = g;
        mem.cdSeen[j] = std::max<int>(mem.cdSeen[j], 4);
    }
}

void checkSymmetry(Controller& ct) {
    if (!SYMMETRY || mapKnown >= 0) return;
    if (symKnown < 0) {
        for (int s = 0; s < 3; s++)
            if (symMatched[s].size() != (size_t)N) symMatched[s].assign(N, 0);
        for (auto const& tile : ct.get_tiles()) {
            int const i = idx(tile.position.x, tile.position.y);
            for (int s = 0; s < 3; s++) {
                if (!symAlive[s]) continue;
                int const j = symImg(s, i);
                if (j == i || mem.lastSeen[j] < 0) continue;
                bool ok = true, feature = false;
                for (int d = 0; d < 4; d++) {
                    int const dj = symDir(s, d);
                    bool const ki = mem.kelp[i] >> d & 1, kj = mem.kelp[j] >> dj & 1;
                    bool const pi = mem.portal[i] >> d & 1, pj = mem.portal[j] >> dj & 1;
                    if (ki != kj || pi != pj) ok = false;
                    feature |= ki || pi;
                }
                // Barren tiles must match too, but don't confirm: open barren ground matches anything.
                if ((mem.spawnAt[i] == -1) != (mem.spawnAt[j] == -1)) ok = false;
                if (!ok) symAlive[s] = false;
                else if (feature && !symMatched[s][i]) {
                    symMatched[s][i] = symMatched[s][j] = 1;
                    symCount[s]++;
                }
            }
        }
        // One left; or all three, each with many matched walls. Two of them imply the third
        // (MX then MY is ROT), so a map with two has all three and any one mirrors right.
        int alive = 0, best = -1, least = INF;
        for (int s = 0; s < 3; s++)
            if (symAlive[s]) {
                alive++;
                least = std::min(least, symCount[s]);
                if (best < 0 || symCount[s] > symCount[best]) best = s;
            }
        if (alive == 1 || (alive == 3 && least >= SYM_CONFIRM)) {
            symKnown = best;
            if (debugOn) ct.output_log("SYM", symKnown, "alive", alive, "matched", symCount[best], "round", T.round);
            mirrorAll();
            return;
        }
    }
    if (symKnown >= 0) {
        for (auto const& tile : ct.get_tiles()) mirrorTile(idx(tile.position.x, tile.position.y));
        mirrorPortals();
    }
}

// Route cost of a crossing onto exit tile e, on top of the step itself. A crossing is blind,
// so it costs by how likely the exit is to be busy: cheap early or when we saw it empty lately,
// full PORTAL_COST when enemy heads were seen by it (a flat 8 lost the shortcut race to trophy's
// centre field and kept us out of schooltime's portal-only third of the income).
inline int portalExtra(int e) {
    if (!PORTAL_RISK) return PORTAL_COST;
    int const R = T.round;
    if (mem.enemyNear[e] >= 0 && R - mem.enemyNear[e] <= PORTAL_DANGER) return PORTAL_COST;
    if (R < PORTAL_EARLY &&
        (!PORTAL_EARLY_SIDE ||
         (symKnown >= 0 && birthTile >= 0 &&
          manhattan(e, birthTile) + PORTAL_EARLY_SIDE <= manhattan(e, symImg(symKnown, birthTile)))))
        return PORTAL_CHEAP;
    if (mem.lastSeen[e] >= 0 && R - mem.lastSeen[e] <= PORTAL_FRESH) return PORTAL_CHEAP;
    return PORTAL_MID;
}

// Can the head enter tile n at step t (1-based) by crossing edge d of tile from?
// The carrier never crosses a portal: whoever crosses can't see the exit, and on
// queen_of_spades our carrier twice crossed onto a teammate's head at the far mouth.
bool avoidPortals = false;

inline bool enterable(int from, int d, int n, int t) {
    if (mem.blocked[from] & (1 << d)) return false;
    if (avoidPortals && mem.exitTo[from * 4 + d] >= 0) return false;
    return T.freeAt[n] <= t;
}

// ---------------------------------------------------------------- evaluation
bool feederMode = false;       // this dragon is walking to the carrier to be eaten
bool scoutMode = false;        // this dragon may cross portals whose exit is unknown
bool scoutRole = false;        // a dedicated scout: goes for vision, not pearls
std::vector<float> prizeMouth; // value of an unmapped portal mouth near rich ground we can't walk to (empty: none)
std::vector<float> huntVal;    // tiles next to big enemy heads, for would-be attackers
std::vector<int> carrierDist;  // wall-aware steps to the carrier's head
int carrierFoodPos = -1;       // carrier head whose nearby pearls we leave to it (-1: none)

float baseTileValue(int c, int t);

// Contested ground: a tile k steps from the symmetry line is 2k from its own image, and a
// tile on the line is exactly as far from each of their spawns as from the matching one of
// ours (devil's centre field, default's portal-only centre). 1 on the line, 0 beyond range.
std::vector<float> contestCache;
int contestKey = -1;
float contest(int c) {
    int const key = symKnown >= 0 ? 8 + symKnown : symAlive[0] | symAlive[1] << 1 | symAlive[2] << 2;
    if (key != contestKey) {
        contestKey = key;
        contestCache.assign(N, 0.f);
        int const range = CONTEST_RANGE > 0 ? CONTEST_RANGE : std::max(4, (W + H) / 8);
        for (int i = 0; i < N; i++) {
            int best = INF;
            for (int s = 0; s < 3; s++)
                if (symKnown >= 0 ? s == symKnown : symAlive[s]) best = std::min(best, manhattan(i, symImg(s, i)));
            contestCache[i] = best >= INF ? 0.f : std::max(0.f, 1.f - best / (2.f * range));
            if (CONTEST_HOME > 0 && birthTile >= 0)
                contestCache[i] *= std::clamp((manhattan(i, birthTile) - CONTEST_HOME) / (float)CONTEST_HOME, 0.f, 1.f);
        }
    }
    return contestCache[c];
}

// The spawn gap a tile's countdowns suggest, once enough of them were seen to trust
// (a countdown near its end looks like a fast tile): -1 unknown.
inline int trustedGap(int c) {
    int const g = mem.cdMax[c];
    return g >= 0 && (g >= 10 || mem.cdSeen[c] >= 4) ? std::max(1, g) : -1;
}

// A scout's view of a tile: new ground and unmapped portals, a pearl only if it's on the way.
float scoutValue(int c, int t) {
    if (T.inVision[c]) return mem.pearl[c] && t <= 2 ? 10.f : 0.f;
    if (mem.lastSeen[c] < 0) return mem.mirrored[c] ? 0.3f : 2.f * mem.exploreBias[c] * (1.f + CONTEST_W / 100.f * contest(c));
    return T.round - mem.lastSeen[c] > 40 ? 0.8f : 0.f;
}

// A scout also values the mouth of a portal nobody has mapped: crossing it opens
// whatever lies behind (on the "portals" map, the pockets and the far half).
float tileValue(int c, int t) {
    float v = scoutRole ? scoutValue(c, t) : baseTileValue(c, t);
    if (T.round < OPENING_ROUNDS && v > 0 && (mem.lastSeen[c] >= 0 || mem.gapHi[c] > 0)) v *= 1.f + CONTEST_W / 100.f * contest(c);
    if (scoutMode && mem.portal[c])
        for (int d = 0; d < 4; d++)
            if (unknownPortal(c, d))
                return std::max({v, SCOUT_VALUE / 10.f, prizeMouth.empty() ? 0.f : prizeMouth[c]});
    return v;
}

float baseTileValue(int c, int t) {
    int const R = T.round;
    if (!giveUp.empty()) {
        auto const it = giveUp.find(c);
        if (it != giveUp.end() && it->second > R) return 0.f;
    }
    // Pearls near the carrier are the carrier's; a feeder leaves them.
    if (feederMode && carrierDist[c] <= 4) return 0.f;
    if (carrierFoodPos >= 0 && manhattan(c, carrierFoodPos) <= CARRIER_FOOD_RADIUS) return 0.f;
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
    if (mem.lastSeen[c] < 0) {
        // Never seen: from its spawn rate if a mirror image or a teammate told us (their
        // farms are raid targets), else curiosity, more of it on contested ground.
        int const g = trustedGap(c);
        if (RATE_VALUE && g > 0) return RATE_DISC / 100.f * 4.5f * std::min(1.f, float(R + t) / std::max(2, g));
        return 1.2f * mem.exploreBias[c] * (1.f + CONTEST_W / 100.f * contest(c));
    }
    if (sp >= 0 && sp <= R + t) return 4.f;
    if (age > 60) return 0.6f * mem.exploreBias[c];
    return 0.f;
}

struct Eval {
    bool legal = false;
    int space = 0;        // time-aware reachable cells (capped)
    float value = 0;      // best target value / distance
    int target = -1;      // the tile that value comes from (debug)
    int threat = 0;
    bool headOn = false;
    bool survive = false; // survival path exists
    bool strict = false;  // ... even if every other head races us for tiles
    bool safe(int need) const { return legal && survive && space >= need; }
};

std::vector<int> bfsDist, bfsQueue, bfsCost;
std::vector<float> regionDecay; // REGION_GAMMA^k

// Flood from the first step dest (reached at step 1), time-aware.
// startCost: route cost of the first step (1, or more through a portal, see PORTAL_COST).
Eval evaluate(int dest, int need, bool wantValue, int startCost = 1) {
    Eval e;
    e.legal = true;
    bfsDist.assign(N, -1);
    bfsCost.assign(N, 0);
    bfsQueue.clear();
    bfsDist[dest] = 1;
    bfsCost[dest] = startCost;
    bfsQueue.push_back(dest);
    float best = 0, region = 0, weight = 0;
    size_t qi = 0;
    int const cap = wantValue ? N : need;
    while (qi < bfsQueue.size() && (int)bfsQueue.size() < cap + 1) {
        int c = bfsQueue[qi++];
        int t = bfsDist[c];
        if (wantValue) {
            if (REGION_W && REGION_AVG) weight += regionDecay[std::min(bfsCost[c], 255)];
            float v = tileValue(c, t);
            if (v > 0) {
                // spread out: tiles next to other ally heads are someone else's
                bool allyCloser = false;
                for (int a : T.allyHeads)
                    if (manhattan(a, c) + 1 < t) { allyCloser = true; break; }
                float const k = allyCloser ? 0.35f : 1.f;
                v *= k;
                float s = T.round < OPENING_ROUNDS ? v / std::sqrt((float)(bfsCost[c] + 1)) : v / (float)(bfsCost[c] + 1);
                if (s > best) best = s, e.target = c;
                if (REGION_W) {
                    // Pearls only: summed over hundreds of empty tiles, the small curiosity
                    // bonus sent dragons on long trips to barren ground (default 1-5).
                    float const curious = REGION_PEARLS && !T.inVision[c] && mem.lastSeen[c] < 0 && mem.gapHi[c] > 0
                                              ? 0.4f * mem.exploreBias[c] * k : 0.f;
                    region += std::max(0.f, v - curious) * regionDecay[std::min(bfsCost[c], 255)];
                }
            }
        }
        for (int d = 0; d < 4; d++) {
            int n = go(c, d);
            if (bfsDist[n] >= 0) continue;
            if (!enterable(c, d, n, t + 1)) continue;
            bfsDist[n] = t + 1;
            bfsCost[n] = bfsCost[c] + 1 + (mem.exitTo[c * 4 + d] >= 0 ? portalExtra(n) : 0);
            bfsQueue.push_back(n);
        }
    }
    e.space = (int)bfsQueue.size();
    // Best single target, plus the unharvested stock in this direction: one pearl two
    // steps away shouldn't outweigh a whole untouched region (autarky: the other side
    // swept its half and out-ate us 42 to 14 in the first 40 rounds).
    // As an average, a direction doesn't win just by having more ground: on maps where
    // every tile spawns (trophy, big_empty) the sum dwarfed everything else.
    if (REGION_AVG) region = weight > 0 ? region / weight : 0.f;
    e.value = best + REGION_W / 100.f * region;
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
        int n = go(c, d);
        if (mem.blocked[c] & (1 << d)) continue;
        if (avoidPortals && mem.exitTo[c * 4 + d] >= 0) continue;
        if (dfsMark[n] == dfsStamp) continue;
        if (T.freeAt[n] > t + 1) continue;
        if (dfsStrict && T.headDist[n] < t + 1) continue;
        if (T.freeAt[n] >= 2 && T.freeAt[n] < INF) return true; // onto our vacated body
        int ex = 0;
        for (int d2 = 0; d2 < 4; d2++) {
            int m = go(n, d2);
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

// Unseen tiles we could still walk to (known portals included, unknown ones not),
// counted up to `cap`. Few left means everything on foot is explored: the unknown
// portals are the frontier now (a sealed pocket), not a detour from it.
int walkableUnseen(int cap) {
    std::vector<uint8_t> seen(N, 0);
    std::vector<int> q{T.head};
    seen[T.head] = 1;
    int unseen = 0;
    for (size_t qi = 0; qi < q.size() && unseen < cap; qi++)
        for (int d = 0; d < 4; d++) {
            if (mem.blocked[q[qi]] & (1 << d)) continue;
            int const n = go(q[qi], d);
            if (seen[n]) continue;
            seen[n] = 1;
            q.push_back(n);
            if (mem.lastSeen[n] < 0 && !mem.mirrored[n]) unseen++; // a mirrored tile is known, not frontier
        }
    return unseen;
}

// Pearls to be had in the dead-end pocket behind the step head -> n, or -1 if n doesn't
// lead into a small empty pocket. A dragon too short to turn round can't back out of a
// dead end, so the safety check refuses it; but the pearls inside make it long enough to
// split or U-turn out (the tail half leaves, the 2-cell stub stays). That is how the teams
// beating us farm dead-end farms at a pearl a round; we circled outside them instead.
int pocketPearls(int n) {
    std::vector<int> q{n};
    std::vector<int> dist(N, -1);
    dist[T.head] = 0;
    dist[n] = 1;
    int pearls = 0;
    for (size_t qi = 0; qi < q.size(); qi++) {
        int const c = q[qi];
        if (T.inVision[c]) {
            if (T.freeAt[c] != 0 && c != n) return -1; // someone's in it
            // A tile respawning by the time we get there counts too: dilemma's farm columns refill
            // every round, so counting only pearls on the ground (with FARM_MIN 3) kept us out of
            // them while the opponent ate each tile 7 times in 60 rounds (eliminated r125, r151).
            int const sp = mem.spawnAt[c];
            if (mem.pearl[c] || (sp >= 0 && sp <= T.round + dist[c])) pearls++;
        } else {
            // Out of sight (walls are known from the library or memory): a tile due to
            // have respawned by the time we get there counts, e.g. a farm's far end.
            int const sp = mem.spawnAt[c];
            bool const lastPearl = mem.lastSeen[c] >= 0 && mem.pearl[c];
            bool const due = sp >= 0 ? sp <= T.round + dist[c] : (mem.gapHi[c] > 0 && mem.gapHi[c] <= 2);
            if (lastPearl || due) pearls++;
        }
        for (int d = 0; d < 4; d++) {
            if (mem.blocked[c] & (1 << d)) continue;
            int const m = go(c, d);
            if (dist[m] >= 0) continue;
            dist[m] = dist[c] + 1;
            q.push_back(m);
            if ((int)q.size() > POCKET_MAX) return -1; // not a pocket: open ground
        }
    }
    return pearls;
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
        int n = go(tail, d);
        if (!seen[n]) { seen[n] = 1; q.push_back(n); }
    }
    for (size_t qi = 0; qi < q.size() && (int)q.size() < cap; qi++)
        for (int d = 0; d < 4; d++) {
            if (mem.blocked[q[qi]] & (1 << d)) continue;
            int n = go(q[qi], d);
            if (!seen[n]) { seen[n] = 1; q.push_back(n); }
        }
    return (int)q.size();
}

// ---------------------------------------------------------------- actions
std::string moveStr;

void recordMove(std::vector<int> const& dirs) {
    histBeforeMove = headHist;
    lastDirs = dirs;
    lastHead = T.head;
    int cur = T.head;
    for (int d : dirs) {
        cur = go(cur, d);
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
            int n = go(at, d);
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
    regionDecay.resize(256);
    for (int k = 0; k < 256; k++) regionDecay[k] = std::pow(REGION_GAMMA / 100.f, (float)k);
    bool born = true;
    int reachArea = -1;     // tiles reachable without portals, once the map is known
    bool handedOver = false; // this dragon was the carrier and U-turned: the child carries on
    bool wasCarrier = false; // this dragon was the carrier last turn
    debugOn = std::getenv("DRAGON_DEBUG") != nullptr;
    initMapCands();

    while (update(ct, game)) {
        T.round = game.get_round_num();
        T.L = ct.get_length();
        T.units = ct.get_unit_count();
        T.head = idx(ct.get_position().x, ct.get_position().y);
        T.headDir = dirIndex(ct.get_dir());
        checkMapCands(ct);
        if (birthTile < 0) birthTile = T.head;
        readVision(ct);
        checkSymmetry(ct);
        for (int e : T.enemyHeads)
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2 + std::abs(dy); dx <= 2 - std::abs(dy); dx++)
                    mem.enemyNear[idx(((px(e) + dx) % W + W) % W, ((py(e) + dy) % H + H) % H)] = T.round;

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
            int const heardSym = m.type == MSG_PORTAL   ? (int)((raw >> 6) & 3)
                                 : m.type == MSG_SOURCE ? (int)((raw >> 14) & 3)
                                                        : m.sym;
            if (heardSym > 0) adoptSymmetry(heardSym - 1);
            if (m.type == MSG_SOURCE) {
                int tl[2], gp[2];
                decodeSources(raw, tl, gp);
                for (int k = 0; k < 2; k++)
                    if (tl[k] >= 0) noteSource(tl[k], gp[k]);
                continue;
            }
            if (m.type == MSG_PORTAL) {
                int id, ea, eb;
                decodePortal(raw, id, ea, eb);
                if (ea >= 0 && eb >= 0) {
                    notePortal(id, ea);
                    notePortal(id, eb);
                }
                continue;
            }
            int r = m.round; // 9 bits of round, reconstruct nearest
            int full = (T.round & ~511) | r;
            if (full > T.round) full -= 512;
            if (T.round - full > 60) continue;
            if (m.type == MSG_CARRIER) {
                bool fresher = full > carrier.round && m.id == carrier.id;
                // For the same carrier only newer news counts, so an old relay of
                // its length can't undo a U-turn handover.
                bool better = (m.id != carrier.id || full >= carrier.round) &&
                              (m.length > carrier.length || (m.length == carrier.length && m.id < carrier.id));
                bool stale = T.round - carrier.round > BEACON_TTL;
                if (fresher || ((better || stale) && full >= T.round - BEACON_TTL)) {
                    carrier = {m.id, m.length, idx(m.x % W, m.y % H), full};
                }
            } else if (m.type == MSG_ENEMY && m.id == 1) {
                if (RALLY && T.round - full <= RALLY_TTL && full > rally.round) rally = {idx(m.x % W, m.y % H), full, m.length};
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
        // A dragon that hears no carrier beacons itself, so the team settles on its longest;
        // hearing that echoed back makes it a candidate only. In a kelp maze each cut-off
        // group elected its own carrier this way, and carriers never split: 34-43% of our
        // dragon-turns on the ladder's default were non-breeding carriers from round 50.
        bool const mayClaim = wasCarrier || king || T.L >= CLAIM_MIN_LEN ||
                              (T.round >= CLAIM_ANY_FROM && T.L >= CLAIM_LATE_LEN);
        bool const iAmCarrier = !handedOver && T.round >= CARRIER_FROM &&
                                ((carrier.id == myId && mayClaim) ||
                                 (T.L >= carrier.length + TAKEOVER_MARGIN && mayClaim) ||
                                 (carrier.id < 0 && (king || (T.L >= CARRIER_MIN_LEN && (mayClaim || !CLAIM_GATE)))));
        wasCarrier = iAmCarrier;
        if (iAmCarrier) carrier = {myId, T.L, T.head, T.round};
        // A carrier we can see: trust our eyes over the beacon.
        if (!iAmCarrier && carrier.id >= 0)
            for (auto const& tile : ct.get_tiles())
                if (auto const* p = tile.get_dragon(); p && p->is_head() && p->get_id() == carrier.id)
                    carrier.pos = idx(tile.position.x, tile.position.y);

        // Sealed regions (portals with no known exit count as walls) hold far fewer dragons than the map
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
        // Escorts: from ESCORT_FROM 1 in ESCORT_MOD small dragons near the carrier keep between it and
        // enemy heads instead of feeding it (our carrier died head-on at L12 and L20 on schooltime).
        bool const escort = ESCORT_MOD > 0 && T.round >= ESCORT_FROM && carrierKnown && myId % ESCORT_MOD == 3 &&
                            T.L <= 4 && carrierD <= ESCORT_RADIUS && carrier.length >= GUARD_MIN_LEN;
        bool const feeder = carrierKnown && !insurance && !born && !escort && carrier.length >= 2 * T.L &&
                            ((endgame && carrier.length > T.L && T.units > std::max(1, KEEP_UNITS)) || hiveFeeder);
        feederMode = feeder || escort; // neither eats the carrier's food
        carrierFoodPos = CARRIER_FOOD_FROM > 0 && T.round >= CARRIER_FOOD_FROM && carrierKnown && T.round - carrier.round <= 6
                             ? carrier.pos
                             : -1;
        if (feeder || escort) {
            // wall-aware distance field from the carrier's head
            carrierDist.assign(N, INF);
            std::vector<int> q{carrier.pos};
            carrierDist[carrier.pos] = 0;
            for (size_t qi = 0; qi < q.size(); qi++) {
                int c = q[qi];
                for (int d = 0; d < 4; d++) {
                    if (mem.blocked[c] & (1 << d)) continue;
                    int n = go(c, d);
                    if (carrierDist[n] != INF) continue;
                    carrierDist[n] = carrierDist[c] + 1;
                    q.push_back(n);
                }
            }
        }

        std::string why;
        double sPart[4][2] = {}; // move score parts (debug): safety tier, threat/flee terms
        bool acted = false;
        std::vector<int> moveDirs;

        // --- local head count: ours in view (us included) vs theirs in view, plus enemy heads
        // seen in the last SEEN_TTL rounds within SEEN_RADIUS (likely just out of view).
        for (size_t k = 0; k < T.enemyHeads.size(); k++) enemySeen[T.enemyHeadId[k]] = {T.enemyHeads[k], T.round};
        int theirHeads = (int)T.enemyHeads.size();
        for (auto it = enemySeen.begin(); it != enemySeen.end();) {
            int const age = T.round - it->second.second;
            if (age > SEEN_TTL) {
                it = enemySeen.erase(it);
                continue;
            }
            if (age > 0 && manhattan(it->second.first, T.head) <= SEEN_RADIUS) theirHeads++;
            ++it;
        }
        int const ourHeads = (int)T.allyHeads.size() + 1;
        bool const tradeRule = TRADE_MODE && theirHeads > 0;
        bool const ahead = tradeRule && ourHeads - theirHeads >= AHEAD_MARGIN;
        bool const behind = tradeRule && theirHeads > ourHeads;

        // --- big enemy heads in view draw our small dragons in for a head-on
        huntVal.clear();
        if (!iAmCarrier && !feeder && T.units >= MIN_UNITS_TO_TRADE && !behind)
            for (size_t k = 0; k < T.enemyHeads.size(); k++) {
                int const le = T.enemyHeadLen[k];
                if (le < std::max(HUNT_MIN_LEN, T.L + 3)) continue;
                if (huntVal.empty()) huntVal.assign(N, 0.f);
                for (int d = 0; d < 4; d++) huntVal[go(T.enemyHeads[k], d)] = 12.f;
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

        // --- rally: rich ground in view with enemy heads on it, where we aren't ahead. Rich is a
        // tile holding a pearl or spawning at least every 20 rounds (library, or countdowns seen);
        // the spot needs 4 such tiles within 3. Schooltime: one crosser at a time into their
        // portal field, 6 of 28 traded on arrival; autarky: they ate our home pockets for 150 rounds.
        auto fastTile = [](int c) {
            int const g = mem.gapHi[c] > 0 ? (mem.gapLo[c] + mem.gapHi[c]) / 2 : trustedGap(c);
            return mem.pearl[c] || (g > 0 && g <= 20);
        };
        if (RALLY && T.round >= RALLY_FROM && !ahead && (int)T.enemyHeads.size() >= RALLY_MIN_ENEMY) {
            int bestSpot = -1, bestScore = 0;
            for (auto const& tile : ct.get_tiles()) {
                int const c = idx(tile.position.x, tile.position.y);
                if (!fastTile(c)) continue;
                int en = 0, rich = 0;
                for (int e : T.enemyHeads) en += manhattan(e, c) <= 3;
                if (en < RALLY_MIN_ENEMY) continue;
                for (auto const& t2 : ct.get_tiles()) {
                    int const c2 = idx(t2.position.x, t2.position.y);
                    rich += manhattan(c2, c) <= 3 && fastTile(c2);
                }
                if (rich >= 4 && en * 16 + rich > bestScore) bestScore = en * 16 + rich, bestSpot = c;
            }
            if (bestSpot >= 0) rally = {bestSpot, T.round, (int)T.enemyHeads.size()};
        }
        bool const rallying = RALLY && rally.pos >= 0 && T.round - rally.round <= RALLY_TTL && !iAmCarrier &&
                              !feeder && !scoutRole && T.L <= 4 && manhattan(T.head, rally.pos) <= RALLY_RANGE;
        if (rallying) {
            if (huntVal.empty()) huntVal.assign(N, 0.f);
            int const c = rally.pos;
            for (int dy = -4; dy <= 4; dy++)
                for (int dx = -4 + std::abs(dy); dx <= 4 - std::abs(dy); dx++) {
                    int const n = idx(((px(c) + dx) % W + W) % W, ((py(c) + dy) % H + H) % H);
                    float const v = RALLY_VALUE / 10.f * (1.f - (std::abs(dx) + std::abs(dy)) / 5.f);
                    huntVal[n] = std::max(huntVal[n], v);
                }
        }

        // --- escorts stand 2-3 from the carrier, on the side of the nearest enemy head in view
        if (escort) {
            if (huntVal.empty()) huntVal.assign(N, 0.f);
            int ne = -1;
            for (int e : T.enemyHeads)
                if (ne < 0 || manhattan(e, carrier.pos) < manhattan(ne, carrier.pos)) ne = e;
            int const c = carrier.pos;
            for (int dy = -3; dy <= 3; dy++)
                for (int dx = -3 + std::abs(dy); dx <= 3 - std::abs(dy); dx++) {
                    int const dist = std::abs(dx) + std::abs(dy);
                    if (dist < 2) continue;
                    int const n = idx(((px(c) + dx) % W + W) % W, ((py(c) + dy) % H + H) % H);
                    float v = 8.f;
                    if (ne >= 0 && manhattan(n, ne) < manhattan(c, ne)) v = 14.f;
                    huntVal[n] = std::max(huntVal[n], v);
                }
        }

        // --- sealed prizes: vision sees through kelp, so rich tiles we can see (or know by mirror or
        // sonar) but can't walk to lie behind portals nobody has mapped. Every small dragon may then
        // cross the unmapped portals near them; the first crossing maps the pair for everyone.
        // Schooltime: a third of the income behind portals, 0 of our head-turns there all game.
        prizeMouth.clear();
        if (PRIZE && mapKnown < 0) {
            std::vector<uint8_t> reach(N, 0);
            std::vector<int> q{T.head};
            reach[T.head] = 1;
            for (size_t qi = 0; qi < q.size(); qi++)
                for (int d = 0; d < 4; d++) {
                    if (mem.blocked[q[qi]] & (1 << d)) continue;
                    int const n = go(q[qi], d);
                    if (!reach[n]) reach[n] = 1, q.push_back(n);
                }
            std::vector<int> prizes;
            for (int c = 0; c < N; c++) {
                if (reach[c]) continue;
                bool const seenNow = T.inVision[c];
                int const g = trustedGap(c);
                if ((seenNow && mem.pearl[c]) || (g > 0 && g <= 20)) mem.prizeAt[c] = T.round;
                if (mem.prizeAt[c] >= 0 && T.round - mem.prizeAt[c] <= PRIZE_TTL) prizes.push_back(c);
            }
            if (!prizes.empty())
                for (int i = 0; i < N; i++) {
                    if (!mem.portal[i] || !reach[i]) continue;
                    bool unknown = false;
                    for (int d = 0; d < 4; d++) unknown |= unknownPortal(i, d);
                    if (!unknown) continue;
                    int near = 0;
                    for (int c : prizes) near += manhattan(i, c) <= PRIZE_RADIUS;
                    if (!near) continue;
                    if (prizeMouth.empty()) prizeMouth.assign(N, 0.f);
                    prizeMouth[i] = PRIZE_VALUE / 10.f * std::min(1.f, near / 4.f);
                }
        }

        // --- carrier: remembered enemy heads (seen lately, out of view now) may be anywhere within 3 + age
        std::vector<uint8_t> memThreat;
        if (iAmCarrier && CARRIER_MEMTHREAT > 0)
            for (auto const& [id, v] : enemySeen) {
                int const age = T.round - v.second;
                if (age <= 0) continue; // in view: T.threatFar covers it
                int const reach = 3 + age, e = v.first;
                if (memThreat.empty()) memThreat.assign(N, 0);
                for (int dy = -reach; dy <= reach; dy++)
                    for (int dx = -reach + std::abs(dy); dx <= reach - std::abs(dy); dx++)
                        memThreat[idx(((px(e) + dx) % W + W) % W, ((py(e) + dy) % H + H) % H)] = 1;
            }

        // --- evaluate the four single steps
        avoidPortals = iAmCarrier && !CARRIER_PORTALS;
        // Dedicated scouts (1 in SCOUT_MOD small dragons) look for new ground while there is any,
        // and cross unmapped portals at once: the first crossing maps the pair for everyone
        // (sonar), and a dead 2-cell scout costs one pearl. Others still wait for the frontier.
        bool hasUnknownPortal = false;
        for (int i = 0; i < N && !hasUnknownPortal; i++)
            if (mem.portal[i])
                for (int d = 0; d < 4; d++)
                    if (unknownPortal(i, d)) hasUnknownPortal = true;
        scoutRole = SCOUT_MOD > 0 && myId % SCOUT_MOD == 5 && !iAmCarrier && !feeder && !endgame && T.L <= 3 &&
                    T.units > LOW_UNITS && (hasUnknownPortal || walkableUnseen(1) > 0);
        scoutMode = PORTALS && !iAmCarrier && !feeder && T.L <= SCOUT_LEN && T.units > LOW_UNITS &&
                    (scoutRole || !prizeMouth.empty() || walkableUnseen(SCOUT_FRONTIER) < SCOUT_FRONTIER);
        Eval ev[4];
        bool scoutDir[4] = {false, false, false, false};
        int const need = std::min(T.L + 2, 60);
        for (int d = 0; d < 4; d++) {
            int n = go(T.head, d);
            if (!enterable(T.head, d, n, 1)) {
                // A scout may cross blind: the exit is out of sight, so its room can't
                // be checked, but the partner edge will be in view on arrival.
                if (scoutMode && unknownPortal(T.head, d)) {
                    ev[d].legal = ev[d].survive = ev[d].strict = true;
                    ev[d].space = need;
                    ev[d].value = std::max(SCOUT_VALUE / 20.f, prizeMouth.empty() ? 0.f : prizeMouth[T.head]); // as a mouth one step away
                    scoutDir[d] = true;
                }
                continue;
            }
            ev[d] = evaluate(n, need, true, 1 + (mem.exitTo[T.head * 4 + d] >= 0 ? portalExtra(n) : 0));
            ev[d].survive = survives(n, std::min(T.L + 2, 40), false);
            ev[d].strict = ev[d].survive && survives(n, std::min(T.L + 2, 40), true);
            if (FARM_ENTRY && !iAmCarrier && !ev[d].safe(need)) {
                int const k = pocketPearls(n);
                if (k >= FARM_MIN && T.L + k >= 4 && tailRoom(4) >= 2) {
                    ev[d].survive = ev[d].strict = true;
                    ev[d].space = std::max(ev[d].space, need);
                }
            }
            for (int e : T.enemyHeads) if (e == n) ev[d].headOn = true;
            for (int a : T.allyHeads) if (a == n) ev[d].legal = false;
            if (ev[d].headOn) ev[d].legal = false; // handled by lunge logic
        }

        // --- 1. lunge into a bigger enemy head (never trade our way into elimination)
        // Guards also trade evenly with any enemy head that gets near our carrier.
        if (!iAmCarrier && !acted && T.units >= std::min(MIN_UNITS_TO_TRADE, 3)) {
            int bestGain = 0;
            std::vector<int> bestPath;
            // Even trades hold ground: a small dragon for an enemy at least as long, where
            // our heads outnumber theirs in view (we get the tiles back, they don't).
            bool const evenOk = TRADE_MODE ? ahead && T.L <= TRADE_LEN && T.units >= MIN_UNITS_TO_TRADE
                                           : TRADE_EVEN > 0 && T.units >= TRADE_EVEN && T.L <= 3 &&
                                                 T.allyHeads.size() + 1 > T.enemyHeads.size();
            bool const downOk = TRADE_DOWN > 0 && tradeRule && ourHeads - theirHeads >= TRADE_DOWN_MARGIN &&
                                T.L <= TRADE_DOWN_LEN && T.units >= MIN_UNITS_TO_TRADE;
            for (size_t k = 0; k < T.enemyHeads.size(); k++) {
                int le = T.enemyHeadLen[k];
                int maxSteps = std::min(T.L - 1, 3);
                if (maxSteps < 1) continue;
                // Every dragon beacons itself until a real carrier exists, so only
                // guard one that is long enough to be worth an even trade.
                bool const guarding = carrierKnown && T.round >= CARRIER_FROM && carrier.length >= GUARD_MIN_LEN &&
                                      manhattan(T.enemyHeads[k], carrier.pos) <= GUARD_RADIUS;
                if (!guarding && T.units < MIN_UNITS_TO_TRADE) continue;
                if (!guarding && behind) continue; // the drops would land among their mouths
                std::vector<int> path;
                if (!findLunge(T.enemyHeads[k], maxSteps, path)) continue;
                int costLen = T.L; // we die too
                int gain = le - costLen;
                if (guarding) gain = std::max(gain, 1) + 100;
                bool const late = T.round > 440 && T.L <= LATE_EVEN_MAXLEN;
                if (gain > bestGain || ((evenOk || (late && le >= 8)) && gain >= 0 && bestPath.empty()) ||
                    (downOk && gain >= -TRADE_DOWN && bestPath.empty())) {
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
        bool const grower = GROWER_MOD > 0 && T.round >= GROW_FROM && myId % GROWER_MOD == 2;
        if (!acted && !iAmCarrier && !grower && !(feeder && carrierD <= FEED_NEAR) && T.round < SPLIT_UNTIL &&
            T.L >= SPLIT_LEN && T.units < popCap && !(born && T.round > 1 && T.L >= 2 * SPLIT_LEN) &&
            ct.can_split(T.L / 2)) {
            int crowd = 0;
            for (auto const& tile : ct.get_tiles())
                if (auto const* p = tile.get_dragon(); p && p->get_team() == myTeam && p->get_id() != myId) crowd++;
            // A big parent needs a safe move next turn too. A small one splits
            // regardless: its child is the tail half facing out, so in a dead-end
            // farm the split is how the swarm gets back out (龙虎豹 on autarky).
            bool anySafe = T.L <= 2 * SPLIT_LEN;
            for (int d = 0; d < 4; d++)
                if (ev[d].safe(need)) anySafe = true;
            // The child must be able to move: unknown tail position (after a blind crossing) passes.
            bool const childRoom = SPLIT_ROOM <= 0 || (int)T.body.size() < T.L || tailRoom(2 * SPLIT_ROOM) >= SPLIT_ROOM;
            if (crowd <= SPLIT_CROWD && anySafe && childRoom) {
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
            bool const disposable = !iAmCarrier && !feeder && T.L <= SMALL_LEN && T.units > LOW_UNITS;
            for (int d = 0; d < 4; d++) sPart[d][0] = sPart[d][1] = 0;
            for (int d = 0; d < 4; d++) {
                if (!ev[d].legal) continue;
                double s = 0;
                if (disposable) {
                    // Top teams (STAR, 龙虎豹) run 2-3 cell dragons straight at food,
                    // dead ends included, and let them die: a dead 2-cell dragon
                    // drops a pearl where the swarm is feeding, and one that grows
                    // to 4 splits, which sends the tail half back out. Only avoid
                    // dying next turn; otherwise a pearl in view beats safety.
                    s += ev[d].space >= 2 ? 1e7 : ev[d].space * 1e3;
                    if (DISPOSE_MODE == 1) {
                        s += ev[d].safe(need) ? 150 : ev[d].survive ? 80 : 0;
                    } else {
                        // Risk it only for the pearl right in front: long runs down
                        // 1-wide corridors (devil) end in a teammate's face, not food.
                        int const n = go(T.head, d);
                        bool const eatsNow = T.inVision[n] && mem.pearl[n];
                        s += (ev[d].survive || eatsNow) ? 5e6 : 0;
                        s += ev[d].safe(need) ? 150 : 0;
                    }
                } else if (ev[d].safe(need)) s += 1e7 + (ev[d].strict ? STRICT_BONUS : 0);
                else if (ev[d].survive) s += 5e6 + ev[d].space * 1e3;
                else s += ev[d].space * 1e3;
                double const s0 = s;
                // A head-on kills both. Stay out of reach when it would be a
                // good trade for them, or when we can't afford to lose anyone.
                if (iAmCarrier && T.threatFar[go(T.head, d)]) s -= 3e5;
                if (!memThreat.empty() && memThreat[go(T.head, d)]) s -= CARRIER_MEMTHREAT;
                if (iAmCarrier && !T.enemyHeads.empty()) {
                    int nd = INF;
                    for (int e : T.enemyHeads) nd = std::min(nd, manhattan(e, go(T.head, d)));
                    s += std::min(nd, 7) * (double)FLEE_WEIGHT;
                }
                if (ev[d].threat) {
                    int const tl = T.threatLen[go(T.head, d)];
                    if (iAmCarrier || T.L >= BIG_THREAT_LEN) s -= BIG_THREAT;
                    else if (cautious) s -= 3e5;
                    else if (T.units <= LOW_UNITS) s -= THREAT_LOW;
                    else if (behind) s -= BEHIND_THREAT;
                    else if (T.L > tl) s -= THREAT_BIGGER;
                    else s -= 2.0;
                }
                // Outnumbered here: back off from their heads, toward ours.
                if (behind && !iAmCarrier && !feeder) {
                    int const n = go(T.head, d);
                    int ne = INF, na = INF;
                    for (int e : T.enemyHeads) ne = std::min(ne, manhattan(e, n));
                    for (int a : T.allyHeads) na = std::min(na, manhattan(a, n));
                    s += std::min(ne, 5) * (double)BEHIND_FLEE;
                    if (na < INF) s -= std::min(na, 8) * (double)BEHIND_FLEE / 2;
                }
                sPart[d][0] = s0, sPart[d][1] = s - s0;
                s += ev[d].value * 100.0;
                // feeders head to the carrier
                if (feeder) s -= carrierDist[go(T.head, d)] * 50.0;
                if (T.allyNear[go(T.head, d)]) s -= 1.0;
                if (LOOP_TABU && std::find(recentHeads.begin(), recentHeads.end(), go(T.head, d)) != recentHeads.end())
                    s -= T.round < stallUntil ? LOOP_TABU_STALL : LOOP_TABU;
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
            bool const eatFirst = trapped && best >= 0 && T.L >= 4 && T.inVision[go(T.head, best)] &&
                                  mem.pearl[go(T.head, best)] && T.units < ct.unit_limit - 1 &&
                                  tailRoom(std::min(T.L + 4, 60)) >= std::min(T.L + 2, 60);
            // A carrier or big dragon U-turns only with no survivable move (each U-turn costs 2 cells, and
            // its own feeders crowd it, so free space often looks short); a newborn never on its first
            // turn: it faces into the old body and U-turned again (16 -> 8 in one round on schooltime).
            // Starting dragons are exempt: some maps start one facing a wall (dilemma's 11-cell dragon has
            // to U-turn on round 1; blocking it lost 0-4), and in the opening a long dragon is no carrier.
            bool const big = iAmCarrier || (T.L >= BIG_UTURN_LEN && T.round >= 100);
            bool const uturnNeeded = big ? best < 0 || !ev[best].survive : trapped;
            bool const newborn = born && T.round > 1;
            if (uturnNeeded && !eatFirst && (BORN_UTURN || !newborn) && T.L >= 4 && ct.can_split(T.L - 2) && T.L - 2 >= 2) {
                // Escape: the old tail leaves as a new dragon; this 2-cell stub stays.
                // Only worth it when the stub really is boxed in.
                bool const stubDoomed = best < 0 || ev[best].space < 3;
                if (stubDoomed || T.L >= 6) {
                    ct.do_split(T.L - 2);
                    why = "UTURN";
                    acted = true;
                    headHist.resize(std::min<int>(headHist.size(), 2));
                    if (iAmCarrier) {
                        // Hand over: beacon a length the child beats by the takeover margin, so the
                        // child (which hears it) takes over at once.
                        handedOver = true;
                        carrier = {myId, T.L - 2 - TAKEOVER_MARGIN, T.head, T.round};
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
                        int const n = go(T.head, d);
                        int kind = 0;
                        if (mem.kelp[T.head] & (1 << d)) kind = -2; // dies alone: better than taking a teammate down
                        else if (std::find(T.enemyHeads.begin(), T.enemyHeads.end(), n) != T.enemyHeads.end()) kind = 3;
                        else if (std::find(T.allyHeads.begin(), T.allyHeads.end(), n) != T.allyHeads.end()) kind = -3;
                        else if (mem.blocked[T.head] & (1 << d)) kind = 1; // portal
                        if (kind > bestKind) { bestKind = kind; best = d; }
                    }
                    if (best < 0) best = 0;
                    why = "DOOMED";
                } else {
                    why = scoutDir[best] ? "SCOUT" : trapped ? "TIGHT" : "MOVE";
                }
                moveDirs = {best};
                acted = true;
            }
        }

        recentHeads.push_back(T.head);
        while (recentHeads.size() > 12) recentHeads.pop_front();
        if (!moveDirs.empty()) {
            std::vector<Direction> ds;
            for (int d : moveDirs) ds.push_back(Direction(DC[d]));
            if (ds.size() == 1) ct.make_move(ds[0]);
            else ct.make_moves(ds);
            recordMove(moveDirs);
        }

        // --- stall detection: circling at most a few tiles for STALL_WIN moves without
        // growing means the targets chased meanwhile can't be had; ignore them a while.
        if (STALL_WIN > 0) {
            if ((why == "MOVE" || why == "TIGHT") && moveDirs.size() == 1) {
                // Next to the target and still not stepping onto it (the step isn't safe):
                // it can't be had from here, so don't circle waiting for it.
                int const tg = ev[moveDirs[0]].target;
                for (int d = 0; d < 4 && tg >= 0; d++)
                    if (go(T.head, d) == tg && d != moveDirs[0] && !ev[d].safe(need)) giveUp[tg] = T.round + GIVEUP_ADJ;
                trail.push_back({T.head, tg});
                trailLen.push_back(T.L);
                // One full circle is enough: a snake of length L revisits a tile after at
                // least L+1 moves, so L+2 moves over <= max(4, L+1) tiles is a loop (ladder:
                // a 3-cell dragon circled a 2x2 for 9 rounds, then walked into a head-on).
                int const win = LOOP_FAST ? std::max(4, T.L + 1) + 1 : STALL_WIN;
                while ((int)trail.size() > std::max(win, STALL_WIN)) trail.pop_front(), trailLen.pop_front();
                int const from = (int)trail.size() - win;
                if (from >= 0 && T.L <= trailLen[from]) {
                    std::vector<int> seen;
                    bool waiting = false; // circling a tile that is about to spawn is farming, not a stall
                    for (int k = from; k < (int)trail.size(); k++) {
                        auto const [h, tg] = trail[k];
                        if (std::find(seen.begin(), seen.end(), h) == seen.end()) seen.push_back(h);
                        if (LOOP_FAST && tg >= 0 && !mem.pearl[tg] && mem.spawnAt[tg] >= 0 &&
                            mem.spawnAt[tg] - T.round <= LOOP_WAIT)
                            waiting = true;
                    }
                    if ((int)seen.size() <= std::max(4, T.L + 1) && !waiting) {
                        for (int k = from; k < (int)trail.size(); k++) {
                            int const tg = trail[k].second;
                            if (tg < 0) continue;
                            giveUp[tg] = T.round + GIVEUP_ROUNDS;
                            for (int d = 0; d < 4; d++) giveUp[step(tg, d)] = T.round + GIVEUP_ROUNDS;
                        }
                        stallUntil = T.round + 10;
                        if (debugOn) ct.output_log("STALL give up", win, "moves");
                        trail.clear(), trailLen.clear();
                    }
                }
            } else {
                trail.clear(), trailLen.clear();
            }
        }

        // --- sonar out: beacon relays the carrier
        if (why != "SACRIFICE") {
            Msg m{MSG_CARRIER, T.round & 511, carrier.id < 0 ? myId : carrier.id,
                  carrier.id < 0 ? T.L : carrier.length,
                  px(carrier.pos < 0 ? T.head : carrier.pos), py(carrier.pos < 0 ? T.head : carrier.pos)};
            m.round = (carrier.round < 0 ? T.round : carrier.round) & 511;
            m.sym = symKnown + 1;
            // Knowing no carrier, a dragon beacons itself, and hearing that echoed back lets it
            // claim; only dragons that could carry should (26 small "carriers" at once on the ladder).
            bool const quiet = QUIET_SMALL && carrier.id < 0 && !king && T.L < CARRIER_MIN_LEN;
            uint64_t v = quiet ? 0 : encode(m);
            uint64_t side = v;
            bool const rallyFresh = RALLY && rally.pos >= 0 && T.round - rally.round <= RALLY_TTL;
            bool const intelOut = enemyBig.pos >= 0 && T.round - enemyBig.round <= INTEL_TTL;
            if ((T.round & 1) && rallyFresh && (T.round % 4 == 1 || !intelOut)) {
                Msg e{MSG_ENEMY, rally.round & 511, 1, std::min(rally.enemies, 255), px(rally.pos), py(rally.pos), symKnown + 1};
                side = encode(e);
            } else if ((T.round & 1) && intelOut) {
                Msg e{MSG_ENEMY, enemyBig.round & 511, 0, enemyBig.length, px(enemyBig.pos), py(enemyBig.pos), symKnown + 1};
                side = encode(e);
            }
            // Fast-spawning tiles we have seen ourselves (off the library): a newborn knows none.
            std::vector<std::pair<int, int>> srcs; // (gap, tile)
            if (SOURCE_SHARE && mapKnown < 0) {
                for (int i = 0; i < N; i++)
                    if (mem.lastSeen[i] >= 0) {
                        int const g = trustedGap(i);
                        if (g > 0 && g <= SOURCE_GAP) srcs.push_back({g, i});
                    }
                std::sort(srcs.begin(), srcs.end());
                if (srcs.size() > 8) srcs.resize(8);
            }
            uint64_t srcFrame = 0;
            if (!srcs.empty()) {
                size_t const k = (size_t)T.round * 2 % srcs.size();
                auto const [ga, a] = srcs[k];
                auto const [gb, b] = srcs.size() > 1 ? srcs[(k + 1) % srcs.size()] : std::pair{0, -1};
                srcFrame = encodeSources(a, ga, b, gb, symKnown + 1);
                if (T.round % 3 == 2 || !v) v = srcFrame; // N/S: every third round the sources, not the beacon
            }
            // Off the library, E/W relay the portal pairs we know on even rounds:
            // a newborn child knows none, and the pairs are the way out of sealed pockets.
            uint64_t sideE = side, sideW = side;
            if (srcFrame && (!(T.round & 1) || !side)) sideE = sideW = srcFrame;
            if (PORTAL_SHARE && !(T.round & 1) && mapKnown < 0) {
                std::vector<int> ids;
                for (auto const& [id, e] : portalSeen)
                    if (e.size() == 2) ids.push_back(id);
                if (!ids.empty()) {
                    std::sort(ids.begin(), ids.end());
                    size_t const k = T.round / 2;
                    int const a = ids[k % ids.size()], b = ids[(k + 1) % ids.size()];
                    sideE = encodePortal(a, portalSeen[a][0], portalSeen[a][1], symKnown + 1);
                    sideW = encodePortal(b, portalSeen[b][0], portalSeen[b][1], symKnown + 1);
                }
            }
            for (int d = 0; d < 4; d++) {
                uint64_t const out = d == 0 || d == 2 ? v : d == 1 ? sideE : sideW;
                if (out) ct.send_sonar(Direction(DC[d]), out);
            }
        }

        ct.set_indicator_string(why + (iAmCarrier ? " C" : ""));
        if (debugOn && iAmCarrier && T.round % 10 == 0)
            ct.output_log("CARR", T.round, "id", myId, "L", T.L, "pearlsInView",
                          (int)std::count_if(ct.get_tiles().begin(), ct.get_tiles().end(), [](Tile const& t) { return t.has_pearl(); }));
        if (debugOn && why == "SACRIFICE") ct.output_log("SAC", T.round, "L", T.L, "cd", manhattan(T.head, carrier.pos));
        if (debugOn && T.round % 50 == 0) ct.output_log("SYMSTATE", symKnown, "alive", symAlive[0], symAlive[1], symAlive[2]);
        if (debugOn) ct.output_log("V10", "rally", (int)rallying, "prize", (int)!prizeMouth.empty(), "escort", (int)escort,
                                   "scout", (int)scoutMode);
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
                dirs += std::string(1, DC[d]) + (ev[d].legal ? "" : "x") + (T.threatFar[go(T.head, d)] ? "!" : "") +
                        (ev[d].safe(need) ? "s" : "") + " ";
            ct.output_log("CDBG head", px(T.head), py(T.head), "enemies", en, "dirs", dirs);
        }
        if (debugOn && why == "MOVE" && !moveDirs.empty()) {
            int const tg = ev[moveDirs[0]].target;
            ct.output_log("TIER", ev[moveDirs[0]].strict ? "strict" : "safe", "target",
                          tg < 0 ? -1 : px(tg), tg < 0 ? -1 : py(tg), "val", ev[moveDirs[0]].value,
                          "tv", tg < 0 ? 0.f : tileValue(tg, 0), "sp", tg < 0 ? 0 : mem.spawnAt[tg] - T.round,
                          "pearl", tg < 0 ? 0 : (int)mem.pearl[tg], "occ", tg < 0 ? 0 : (T.freeAt[tg] != 0));
        }
        // Backing off: an enemy in view and the chosen step isn't the best-valued safe one.
        if (debugOn && why == "MOVE" && moveDirs.size() == 1 && !T.enemyHeads.empty()) {
            int const c = moveDirs[0];
            int bv = -1;
            for (int d = 0; d < 4; d++)
                if (ev[d].safe(need) && (bv < 0 || ev[d].value > ev[bv].value)) bv = d;
            char const* r = "none";
            if (bv >= 0 && bv != c && ev[bv].value > ev[c].value + 0.05f)
                r = sPart[c][0] > sPart[bv][0] ? "strict" : sPart[c][1] > sPart[bv][1] ? (iAmCarrier ? "flee" : "threat") : "other";
            ct.output_log("YIELD", r, "L", T.L, "carrier", (int)iAmCarrier, "lost", bv >= 0 ? ev[bv].value - ev[c].value : 0.f);
        }
        if (debugOn && (why == "DOOMED" || why == "TIGHT" || why == "UTURN")) {
            std::string b;
            for (int i : T.body) b += std::to_string(px(i)) + "," + std::to_string(py(i)) + " ";
            ct.output_log("L", T.L, "head", px(T.head), py(T.head), "body", b);
            for (int d = 0; d < 4; d++) {
                char const* who = "none";
                if (auto const* tile = ct.get_tile(Position(px(go(T.head, d)), py(go(T.head, d)))))
                    if (auto const* p = tile->get_dragon())
                        who = p->get_id() == myId ? "own" : p->get_team() == myTeam ? "ally" : "enemy";
                ct.output_log(DC[d], "legal", ev[d].legal, "space", ev[d].space, "surv", ev[d].survive,
                              "blk", (int)mem.blocked[T.head], "free", T.freeAt[go(T.head, d)], "who", who);
            }
        }
        born = false;
        end_turn();
    }
    (void)born;
}
