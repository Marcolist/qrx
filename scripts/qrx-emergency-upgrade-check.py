#!/usr/bin/env python3
import argparse,json,sys
ap=argparse.ArgumentParser(description='Fail-closed QRX emergency upgrade readiness checker')
ap.add_argument('--current-height',type=int,required=True); ap.add_argument('--activation-height',type=int,required=True); ap.add_argument('--min-lead-blocks',type=int,default=360)
ap.add_argument('--agreement',type=float,required=True,help='observed upgraded voting-power fraction 0..1')
a=ap.parse_args(); lead=a.activation_height-a.current_height
ok=lead>=a.min_lead_blocks and a.agreement>2/3
print(json.dumps({'ready':ok,'current_height':a.current_height,'activation_height':a.activation_height,'lead_blocks':lead,'minimum_lead_blocks':a.min_lead_blocks,'upgraded_voting_power_fraction':a.agreement,'requires_strictly_more_than_two_thirds':True},indent=2))
sys.exit(0 if ok else 2)
