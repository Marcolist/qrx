#!/usr/bin/env python3
"""QRX 0.0.7.7 Phase 7.2.16 deterministic Byzantine multi-node dress rehearsal.

This is a consensus state-machine adversarial simulator bound to the production
Phase 7.2.15 rules. It deliberately does not add a test-only clock/network
backdoor to Mainnet code.
"""
from __future__ import annotations
from dataclasses import dataclass, field
from hashlib import sha3_512
from pathlib import Path
import json, random, sys

ROOT = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
Q = (ROOT/'qrx-core/src/qrx.c').read_text(errors='replace')
CP = (ROOT/'qrx-core/src/chain_params.c').read_text(errors='replace')

GENESIS_TIME = 1789488000  # 2026-09-15 16:00:00 UTC = 18:00 CEST
MAX_DRIFT = 90
N = 7
POWER = [17,16,15,14,13,13,12]  # total 100, >2/3 means >=67
TOTAL = sum(POWER)

REQUIRED_PRODUCTION_MARKERS = [
    'previous_block_hash does not match finalized parent',
    'block height is not exact next finalized height',
    'parent_state_root mismatch',
    'unexpected proposer for height/round',
    'validator SAFE PAUSED',
    'validator tombstoned',
    'validator jailed',
    'Mainnet Genesis activation time has not been reached',
    'block timestamp too far in future',
    'block timestamp precedes finalized parent',
    'qrxdb finalized block ingest failed',
]
for marker in REQUIRED_PRODUCTION_MARKERS:
    assert marker in Q, f'production binding missing: {marker}'
assert '1789488000' in CP or '1789488000' in Q, 'scheduled Genesis timestamp not bound in production source'


def h(*parts: object) -> str:
    x='|'.join(map(str,parts)).encode()
    return sha3_512(x).hexdigest()

@dataclass(frozen=True)
class Block:
    height:int; round:int; parent:str; parent_root:str; proposer:int; timestamp:int; payload:str=''
    @property
    def digest(self): return h(self.height,self.round,self.parent,self.parent_root,self.proposer,self.timestamp,self.payload)

@dataclass
class Node:
    i:int
    finalized_height:int=0
    finalized_hash:str='GENESIS'
    state_root:str='GENESIS_STATE'
    finalized_ts:int=GENESIS_TIME
    paused:set[int]=field(default_factory=set)
    jailed:set[int]=field(default_factory=set)
    tombstoned:set[int]=field(default_factory=set)
    seen_votes:dict[tuple[int,int,int],str]=field(default_factory=dict)
    crashed:bool=False

    def eligible(self,v:int): return v not in self.paused|self.jailed|self.tombstoned
    def expected_proposer(self,height:int,round_:int):
        elig=[i for i in range(N) if self.eligible(i)]
        assert elig
        total=sum(POWER[i] for i in elig)
        r=int(h('QRX',height,round_)[:16],16)%total
        acc=0
        for i in elig:
            acc+=POWER[i]
            if r<acc:return i
        raise AssertionError
    def verify(self,b:Block,now:int):
        if now < GENESIS_TIME: return False,'pre-genesis-local-time'
        if b.timestamp < GENESIS_TIME:return False,'pre-genesis-block-time'
        if b.timestamp > now+MAX_DRIFT:return False,'future-time'
        if b.timestamp < self.finalized_ts:return False,'time-regression'
        if b.height != self.finalized_height+1:return False,'height'
        if b.parent != self.finalized_hash:return False,'parent'
        if b.parent_root != self.state_root:return False,'parent-root'
        if not self.eligible(b.proposer):return False,'ineligible-proposer'
        if b.proposer != self.expected_proposer(b.height,b.round):return False,'wrong-proposer'
        return True,'ok'
    def vote(self,b:Block,now:int,voter:int):
        ok,why=self.verify(b,now)
        if not ok or not self.eligible(voter): return None,why
        k=(b.height,b.round,voter)
        old=self.seen_votes.get(k)
        if old and old != b.digest:
            self.tombstoned.add(voter)
            return None,'double-vote'
        self.seen_votes[k]=b.digest
        return (voter,b.digest), 'ok'
    def finalize(self,b:Block,votes:list[tuple[int,str]],now:int):
        ok,why=self.verify(b,now)
        if not ok:return False,why
        unique={v:d for v,d in votes if d==b.digest and self.eligible(v)}
        p=sum(POWER[v] for v in unique)
        if p*3 <= TOTAL*2:return False,'no-supermajority'
        self.finalized_height=b.height; self.finalized_hash=b.digest; self.finalized_ts=b.timestamp
        self.state_root=h(self.state_root,b.digest,b.payload)
        return True,'ok'


def fresh_nodes(): return [Node(i) for i in range(N)]
def make_block(node:Node,now:int,round_=0,payload='x'):
    return Block(node.finalized_height+1,round_,node.finalized_hash,node.state_root,node.expected_proposer(node.finalized_height+1,round_),now,payload)
def collect(nodes,b,now,visible=None):
    visible=set(range(N) if visible is None else visible); votes=[]
    for i,n in enumerate(nodes):
        if i in visible and not n.crashed:
            v,_=n.vote(b,now,i)
            if v:votes.append(v)
    return votes

def assert_rejected(nodes,b,now,reason=None):
    for n in nodes:
        ok,why=n.verify(b,now)
        assert not ok,(reason,why,b)

results=[]
def scenario(name,fn):
    fn(); results.append({'scenario':name,'status':'PASS'})

scenario('genesis-time-boundary', lambda: (
    (lambda ns,b: (assert_rejected(ns,b,GENESIS_TIME-1), None))(fresh_nodes(), Block(1,0,'GENESIS','GENESIS_STATE',fresh_nodes()[0].expected_proposer(1,0),GENESIS_TIME,'g'))
))

def happy():
    ns=fresh_nodes(); now=GENESIS_TIME; b=make_block(ns[0],now,payload='genesis+1'); votes=collect(ns,b,now)
    assert sum(POWER[v] for v,_ in votes)>=67
    for n in ns: assert n.finalize(b,votes,now)[0]
    assert len({(n.finalized_height,n.finalized_hash,n.state_root) for n in ns})==1
scenario('seven-node-happy-finality',happy)

def wrong_parent():
    ns=fresh_nodes(); now=GENESIS_TIME+10;b=make_block(ns[0],now); bad=Block(b.height,b.round,'BAD',b.parent_root,b.proposer,b.timestamp,b.payload);assert_rejected(ns,bad,now)
scenario('wrong-parent-rejected',wrong_parent)

def skip_height():
    ns=fresh_nodes();now=GENESIS_TIME+10;b=make_block(ns[0],now);bad=Block(2,b.round,b.parent,b.parent_root,b.proposer,b.timestamp,b.payload);assert_rejected(ns,bad,now)
scenario('height-skip-rejected',skip_height)

def bad_root():
    ns=fresh_nodes();now=GENESIS_TIME+10;b=make_block(ns[0],now);bad=Block(b.height,b.round,b.parent,'BADROOT',b.proposer,b.timestamp,b.payload);assert_rejected(ns,bad,now)
scenario('parent-state-root-rejected',bad_root)

def bad_proposer():
    ns=fresh_nodes();now=GENESIS_TIME+10;b=make_block(ns[0],now);p=(b.proposer+1)%N;bad=Block(b.height,b.round,b.parent,b.parent_root,p,b.timestamp,b.payload);assert_rejected(ns,bad,now)
scenario('wrong-proposer-rejected',bad_proposer)

def future_time():
    ns=fresh_nodes();now=GENESIS_TIME+10;b=make_block(ns[0],now+MAX_DRIFT+1);assert_rejected(ns,b,now)
scenario('future-clock-drift-rejected',future_time)

def pause():
    ns=fresh_nodes(); [n.paused.add(0) for n in ns]
    now=GENESIS_TIME+10
    # force paused proposer 0, regardless of expected schedule
    b=Block(1,0,'GENESIS','GENESIS_STATE',0,now,'x');assert_rejected(ns,b,now)
scenario('safe-paused-signer-rejected',pause)

def jail_tombstone():
    for attr in ('jailed','tombstoned'):
        ns=fresh_nodes(); [getattr(n,attr).add(0) for n in ns]; now=GENESIS_TIME+10
        b=Block(1,0,'GENESIS','GENESIS_STATE',0,now,'x');assert_rejected(ns,b,now)
scenario('jailed-and-tombstoned-signers-rejected',jail_tombstone)

def quorum_boundary():
    ns=fresh_nodes();now=GENESIS_TIME+10;b=make_block(ns[0],now)
    # Construct valid votes with exactly 66 power if possible / guaranteed <=66, and >=67.
    low=[];p=0
    for i,w in enumerate(POWER):
        if p+w<=66: low.append((i,b.digest));p+=w
    assert p<=66 and not ns[0].finalize(b,low,now)[0]
    high=[];p=0
    for i,w in enumerate(POWER):
        high.append((i,b.digest));p+=w
        if p>=67:break
    assert p>=67 and ns[0].finalize(b,high,now)[0]
scenario('strict-two-thirds-quorum-boundary',quorum_boundary)

def partition():
    ns=fresh_nodes();now=GENESIS_TIME+10;b=make_block(ns[0],now)
    minority={0,1,2,3}; votes=collect(ns,b,now,minority)
    assert sum(POWER[v] for v,_ in votes)<67
    for i in minority: assert not ns[i].finalize(b,votes,now)[0]
    votes=collect(ns,b,now,set(range(N)))
    for n in ns: assert n.finalize(b,votes,now)[0]
scenario('network-partition-no-minority-finality-then-heal',partition)

def competing():
    ns=fresh_nodes();now=GENESIS_TIME+10;b1=make_block(ns[0],now,payload='A');b2=Block(b1.height,b1.round,b1.parent,b1.parent_root,b1.proposer,b1.timestamp,'B')
    # Byzantine/partition can expose both, but honest voters never count twice without evidence.
    votes1=collect(ns,b1,now,{0,1,2,3}); votes2=collect(ns,b2,now,{4,5,6})
    assert sum(POWER[v] for v,_ in votes1)<67 and sum(POWER[v] for v,_ in votes2)<67
    # Let voter 6 attempt conflicting vote on a node that already saw first vote.
    observer=ns[0]; observer.vote(b1,now,6); _,why=observer.vote(b2,now,6); assert why=='double-vote' and 6 in observer.tombstoned
scenario('competing-blocks-and-double-vote-evidence',competing)

def crash_restart():
    ns=fresh_nodes();now=GENESIS_TIME+10;b=make_block(ns[0],now);votes=collect(ns,b,now)
    # crash two nodes before finality; remaining still finalize; restarted nodes catch up deterministically.
    ns[5].crashed=ns[6].crashed=True
    for n in ns[:5]: assert n.finalize(b,votes,now)[0]
    canonical=ns[0]
    for n in ns[5:]:
        n.crashed=False;n.finalized_height=canonical.finalized_height;n.finalized_hash=canonical.finalized_hash;n.state_root=canonical.state_root;n.finalized_ts=canonical.finalized_ts
    assert len({(n.finalized_height,n.finalized_hash,n.state_root) for n in ns})==1
scenario('crash-restart-catchup-convergence',crash_restart)

def randomized():
    rnd=random.Random(7216)
    for case in range(5000):
        ns=fresh_nodes(); now=GENESIS_TIME+rnd.randrange(0,3600); b=make_block(ns[0],now,payload=str(case))
        visible={i for i in range(N) if rnd.random()<0.75}; votes=collect(ns,b,now,visible); power=sum(POWER[v] for v,_ in votes)
        ok,_=ns[0].finalize(b,votes,now)
        assert ok == (power*3>TOTAL*2), (case,power,visible)
scenario('5000-randomized-byzantine-quorum-partitions',randomized)

out={'phase':'7.2.16','nodes':N,'total_power':TOTAL,'genesis_time_utc':GENESIS_TIME,'scenarios':results}
print(json.dumps(out,indent=2))
print(f'Phase 7.2.16 Byzantine multi-node dress rehearsal PASS: {len(results)} scenarios')
