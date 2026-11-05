#!/usr/bin/env python3
"""Genera scenari, non risultati simulati. Nessuna applicazione o rete automatica."""
import argparse
import json
from pathlib import Path
from osc_config import preset


def scenarios(ip):
    for destinations in (1,2,4,8):
        for messages in (4,8,16):
            for hz in (25,50,100):
                config=preset('Custom',ip)
                config['destinations']=[dict(ip=ip,port=16888+i) for i in range(destinations)]
                config['messages']=[dict(address='/bench/{id}/'+str(i), fields=['yaw','pitch','roll'],
                                         destinations=list(range(destinations)), hz=hz,bundle=True)
                                    for i in range(messages)]
                yield dict(name=f'd{destinations}-m{messages}-{hz}hz',config=config,
                           result='NOT RUN',note='richiede validazione firmware, heap e misura hardware')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ip',required=True,help='IP LAN Mac')
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open('x') as stream:
        json.dump(list(scenarios(a.ip)),stream,indent=2)
    print('36 scenari NOT RUN:',a.output)


if __name__=='__main__':
    main()
