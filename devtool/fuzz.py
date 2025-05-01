import argparse
import random
import re
import requests
import subprocess

poocs_template = """#ifndef POOCS_H
#define POOCS_H

static char poocs_xml[] = {
  DATA
};


#endif"""

run_w = [0]
run_results = []

def calc_score():
    unique_t = set()
    score = 0
    t0 = 0
    for w, t in zip(run_w, run_results):
        if t is None:
            continue
        unique_t.add(t)
        if w == 0:
            t0 = t
        elif t0 == 0:
            # failed to solve on s794-stable
            return 0
        elif t != t0:
            score += w
    return score + len(unique_t)

def main():
    designid = 12706540
    for _ in range(1000000):
        designid -= random.randint(1, 10)
        print(f'Using design ID {designid}')

        print(f'Fetching design data')
        URL = "http://www.fantasticcontraption.com/retrieveLevel.php"
        PARAMS = {'id':designid, "loadDesign":1}
        r = requests.post(URL, data = PARAMS).text
        print(f'Design data OK')

        print(f'Encoding to poocs.h')
        encoded = ', '.join(str(ord(c)) for c in r)
        poocs = poocs_template.replace('DATA', encoded)

        with open('src/poocs.h', 'w') as file:
            file.write(poocs)
        print(f'poocs.h written')

        for w in run_w:
            print(f'Building for design id {designid}')
            subprocess.run(['scons', '--autorun=1000'])
            print(f'Running until tick limit')
            proc = subprocess.run(['./fcsim-fpatan'], capture_output=True, text=True)
            print(proc.stdout)
            i_run_result = None
            try:
                i_run_result = int(proc.stdout.strip())
                print(f'Finished in {i_run_result} ticks')
            except:
                print('Reached tick limit without finishing')
            run_results.append(i_run_result)
            if w == 0 and i_run_result is None:
                print('Did not solve on 794-stable. Disqualified')
                break

        print('All runs done')
        print(f'# Table for {designid}')
        for w, t in zip(run_w, run_results):
            print(f'{designid}\t{w}\t{t}')
        print(f'# Score for {designid} is {calc_score()}')

if __name__ == "__main__":
    main()