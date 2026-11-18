"""Summarise .map files: connectivity with/without portals, pearl density."""
import sys, pathlib, collections
def load(p):
    W=H=0; tiles={}; edges={}; dragons=[]
    for line in open(p):
        t=line.split()
        if not t: continue
        if t[0]=="MAP": W,H=int(t[1]),int(t[2])
        elif t[0]=="TILE": tiles[(int(t[1]),int(t[2]))]=(int(t[3]),int(t[4]))
        elif t[0]=="EDGE": edges[int(t[1])]=(int(t[2]),int(t[3]))
        elif t[0]=="DRAGON":
            n=int(t[2]); c=list(map(int,t[3:3+2*n])); dragons.append((int(t[1]),[(c[2*i],c[2*i+1]) for i in range(n)]))
    return W,H,tiles,edges,dragons
def edge_kind(W,H,edges,x,y,d):
    # returns (kind,pid) of edge leaving (x,y) in dir d (N,E,S,W)
    if d=='N': idx=(2*y)*(W+1)+x
    elif d=='S': idx=(2*((y+1)%H))*(W+1)+x if y+1<H else (2*H)*(W+1)+x
    elif d=='W': idx=(2*y+1)*(W+1)+x
    else: idx=(2*y+1)*(W+1)+(x+1)
    k=edges.get(idx,(0,-1))
    if k==(0,-1):
        # wrap alias
        if d=='S' and y+1==H: k=edges.get(x,(0,-1))
        if d=='N' and y==0: k=edges.get((2*H)*(W+1)+x,(0,-1))
        if d=='E' and x+1==W: k=edges.get((2*y+1)*(W+1),(0,-1))
        if d=='W' and x==0: k=edges.get((2*y+1)*(W+1)+W,(0,-1))
    return k
D={'N':(0,-1),'E':(1,0),'S':(0,1),'W':(-1,0)}
def summary(paths):
  for p in paths:
      W,H,tiles,edges,dr=load(p)
      kinds=collections.Counter(k for k,_ in edges.values())
      start=dr[0][1][0]
      seen={start}; q=[start]
      while q:
          x,y=q.pop()
          for d,(dx,dy) in D.items():
              k,_=edge_kind(W,H,edges,x,y,d)
              if k!=0: continue
              n=((x+dx)%W,(y+dy)%H)
              if n not in seen: seen.add(n); q.append(n)
      spawn=[(a,b) for (a,b) in tiles.values() if b>0]
      rate=sum(2/(a+b) for a,b in spawn)
      enemy=[d for t,d in dr if t==1][0][0]
      print(f"{pathlib.Path(p).stem:16s} {W}x{H} kelp={kinds[1]} portal={kinds[2]} reach(no portal)={len(seen)}/{W*H} enemyReach={enemy in seen} pearls/round~{rate:.1f} dragons={[ (t,len(d)) for t,d in dr]}")

def render(p):
    W,H,tiles,edges,dr=load(p)
    occ={}
    for i,(t,segs) in enumerate(dr):
        for j,(x,y) in enumerate(segs):
            occ[(x,y)]=('AB'[t] if j==0 else 'ab'[t])
    out=[]
    for y in range(H+1):
        row=""
        for x in range(W):
            k,_=edges.get((2*y)*(W+1)+x,(0,-1))
            row+="+"+("--" if k==1 else "PP" if k==2 else "  ")
        out.append(row+"+")
        if y==H: break
        row=""
        for x in range(W+1):
            k,_=edges.get((2*y+1)*(W+1)+x,(0,-1))
            row+=("|" if k==1 else "P" if k==2 else " ")
            if x<W:
                mn,mx=tiles.get((x,y),(0,0))
                c=occ.get((x,y)," ")
                row+=c+("." if mx>0 and mx<=60 else " " if mx==0 else ",")
        out.append(row)
    print("\n".join(out))


if __name__=="__main__":
    if sys.argv[1]=="--render": render(sys.argv[2])
    else: summary(sys.argv[1:])
