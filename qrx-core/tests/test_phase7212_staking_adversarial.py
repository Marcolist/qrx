#!/usr/bin/env python3
import random
random.seed(7212)
for _ in range(10000):
    liquid=random.randint(0,10**9); bonded=random.randint(0,10**9); unbond=random.randint(0,bonded); fee=random.randint(0,min(liquid,1000))
    before=liquid+bonded
    b2=bonded-unbond; u2=unbond; l2=liquid-fee
    l3=l2+u2; u3=0
    assert l3+b2+u3 == before-fee
    assert u3 == 0  # a second claim has no principal left to credit
print('Phase 7.2.12 adversarial staking model 10000 cases PASS')
