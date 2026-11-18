"""Classify team A's deaths in tools/runlog.py traces, and break down genuine traps.

    python3 tools/trapstats.py DIR     # every DIR/*.txt trace; team A should run with DRAGON_DEBUG=1

Categories: deliberate (sacrifice, lunge, U-turn stub), newborn first move, DOOMED (no legal
move), and deaths on ordinary moves. For DOOMED it reports what boxed the dragon in (from the
debug "who" field: enemy > ally > walls/own body), whether it ate on the way in, and the label
the turn before. Example setup: tools/variant.sh dbg BIN DRAGON_DEBUG=1, then runlog.py per map/seed.
"""
import re,sys,collections,glob,os
INF=536870912
turn=re.compile(r'^(\d+)\tdragon (\d+) at \((\d+),(\d+)\) len (\d+) team (\w) -> (\S+)(?: (\S+))? \[(.*)\]')
logl=re.compile(r'^(\d+)\tdragon (\d+) log:(.*)')
die=re.compile(r'^(\d+)\tdragon (\d+) dies team (\w) reason (.*)')
spawnre=re.compile(r'^(\d+)\tdragon (\d+) spawns team (\w)')
cat=collections.Counter(); mass=collections.Counter(); permap=collections.defaultdict(collections.Counter)
block=collections.Counter(); prevlab=collections.Counter(); lens=collections.Counter(); rounds=collections.Counter()
tightrun=collections.Counter()
games=0
for f in sorted(glob.glob(sys.argv[1]+'/*.txt')):
    games+=1; mp=os.path.basename(f).rsplit('-',1)[0]
    hist=collections.defaultdict(list); logs=collections.defaultdict(list); born={}
    for l in open(f):
        m=spawnre.match(l)
        if m: born[int(m[2])]=int(m[1]); continue
        m=turn.match(l)
        if m:
            if m[6]!='A': continue
            hist[int(m[2])].append((int(m[1]),int(m[5]),m[7],m[9].replace(' C','')))
            continue
        m=logl.match(l)
        if m: logs[(int(m[1]),int(m[2]))].append(m[3]); continue
        m=die.match(l)
        if not m or m[3]!='A': continue
        r,d,why=int(m[1]),int(m[2]),m[4]
        h=hist.get(d,[])
        if not h: continue
        _,L,act,lab=h[-1]
        prev=h[-2][3] if len(h)>1 else 'NEWBORN'
        if lab=='SACRIFICE': c='sacrifice (deliberate)'
        elif prev=='UTURN': c='U-turn stub (deliberate)'
        elif len(h)==1 and born.get(d,0)>1: c='newborn, first move'
        elif lab=='DOOMED': c='DOOMED (no legal move)'
        elif lab=='TIGHT': c='TIGHT (no safe move, died moving)'
        elif lab=='LUNGE': c='lunge (deliberate trade)'
        else: c=f'died on a {lab} move ({why})'
        cat[c]+=1; mass[c]+=L; permap[mp][c]+=1
        if c=='DOOMED (no legal move)':
            lens[min(L,10)]+=1; rounds[r//100*100]+=1; prevlab[prev]+=1
            # how many consecutive TIGHT turns before
            k=0
            for t in reversed(h[:-1]):
                if t[3] in ('TIGHT',): k+=1
                else: break
            tightrun[min(k,5)]+=1
            # what blocks each direction
            parts=collections.Counter()
            for ln in logs.get((r,d),[]):
                mm=re.match(r'([NESW]) legal (\d) space \d+ surv \d blk (\d+) free (\d+)',ln)
                if not mm: continue
                dd='NESW'.index(mm[1]); blk=int(mm[3]); fr=int(mm[4])
                if blk>>dd&1: parts['wall']+=1
                elif fr>=INF: parts['other dragon']+=1
                elif fr>=2: parts['own body']+=1
                else: parts['open-but-illegal (head)']+=1
            key=tuple(sorted(k for k,v in parts.items() for _ in range(v)))
            has_other='other dragon' in parts
            block['boxed with another dragon' if has_other else 'boxed by walls + own body only']+=1
print(f"{games} games, team A. Deaths by category (count, cells lost):")
for c,v in cat.most_common(): print(f"  {v:4d}  {mass[c]:5d} cells  {c}")
print("\nDOOMED (genuine traps): length",sorted(lens.items()),"\n  by round",sorted(rounds.items()))
print("  label on the turn before:",dict(prevlab))
print("  consecutive TIGHT turns before DOOMED:",sorted(tightrun.items()))
print("  what boxed it:",dict(block))
print("\nper map:")
for mp,c in sorted(permap.items()):
    print(f"  {mp:16s}", {k.split(' (')[0]:v for k,v in c.most_common(6)})

# ---- trap breakdown
turn=re.compile(r'^(\d+)\tdragon (\d+) at \((\d+),(\d+)\) len (\d+) team (\w) -> (\S+)(?: (\S+))? \[(.*)\]')
logl=re.compile(r'^(\d+)\tdragon (\d+) log:(.*)')
die=re.compile(r'^(\d+)\tdragon (\d+) dies team (\w) reason (.*)')
spawnre=re.compile(r'^(\d+)\tdragon (\d+) spawns team (\w)')
kind=collections.Counter(); ate=collections.Counter(); life=collections.Counter(); where=collections.Counter()
sudden=collections.Counter(); pathlen=collections.Counter(); total=0
for f in sorted(glob.glob(sys.argv[1]+'/*.txt')):
    hist=collections.defaultdict(list); logs=collections.defaultdict(list); born={}
    for l in open(f):
        m=spawnre.match(l)
        if m: born[int(m[2])]=int(m[1]); continue
        m=turn.match(l)
        if m:
            if m[6]=='A': hist[int(m[2])].append((int(m[1]),int(m[5]),m[9].replace(' C',''),(int(m[3]),int(m[4]))))
            continue
        m=logl.match(l)
        if m: logs[(int(m[1]),int(m[2]))].append(m[3]); continue
        m=die.match(l)
        if not m or m[3]!='A': continue
        r,d=int(m[1]),int(m[2]); h=hist.get(d,[])
        if len(h)<2 or h[-1][2]!='DOOMED' or h[-2][2]=='UTURN': continue
        total+=1
        who=collections.Counter()
        for ln in logs.get((r,d),[]):
            mm=re.match(r'([NESW]) legal .*blk (\d+) free (\d+) who (\w+)',ln)
            if not mm: continue
            dd='NESW'.index(mm[1])
            if int(mm[2])>>dd&1: who['wall']+=1
            else: who[mm[4]]+=1
        k=('enemy' if who['enemy'] else 'ally' if who['ally'] else 'walls+own')
        kind[k]+=1
        # did it grow in the last 6 turns (ate on the way in)?
        L0=[t[1] for t in h[-7:]]
        grew = L0[-1] > min(L0)
        ate[(k,'ate on the way in' if grew else 'no food')]+=1
        # prior turns: how long since last non-TIGHT label, sudden = previous label MOVE
        prev=h[-2][2]; sudden[(k,prev)]+=1
        age=r-born.get(d,0); life['<=5' if age<=5 else '<=20' if age<=20 else '>20']+=1
print(f"{total} genuine DOOMED deaths (U-turn stubs excluded)")
print("boxed in by (any enemy segment > any ally segment > only walls/own body):",dict(kind))
print("ate on the way in:",dict(ate))
print("label the turn before:",dict(sudden))
print("age at death (rounds since birth):",dict(life))
