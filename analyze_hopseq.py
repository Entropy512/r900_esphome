#!/usr/bin/env python3

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import argparse

ap = argparse.ArgumentParser()
ap.add_argument('-i', '--input', nargs='+', type=argparse.FileType('rb'), required=True,
    help='path to input files')
ap.add_argument('-m', '--meter_id', type=int, required=True, help='Meter ID to filter on - required since we can only reliably analyze the hop sequence of a single meter')

args = vars(ap.parse_args())

#These are derived from analysis, may be slightly off from actual Neptune design
chanlist = 911.08797 + np.arange(17)*0.131072
chanlist = np.append(chanlist, 914.857+np.arange(33)*0.131072)

firstfile = True
for infile in args['input']:
    filedata = pd.read_csv(infile, index_col='time')
    filedata.index = pd.to_datetime(filedata.index)
    filedata = filedata[filedata['id'] == args['meter_id']]
    filedata = filedata['freq']
    if(firstfile):
        hopdata = filedata
        firstfile = False
    else:
        hopdata = pd.concat([hopdata, filedata])

firsttime = hopdata.index[0]
hopdata.index = (hopdata.index - firsttime)/pd.to_timedelta(1, unit='s')
hopdata.index = hopdata.index % 700
hopdata.sort_index(inplace=True)

for chan in chanlist:
    plt.axhline(chan, color='r', alpha=0.2)

plt.scatter(hopdata.index, hopdata, label="Observed Transmissions")

hopchan = 1 #My current dataset has the first transmission seen on channel 1.  FIXME:  Don't hardcode this
timepdelta = 14
hoptime = 0
hoptimes = []
hopchans = []
while(hoptime < 700):
    hoptimes.append(hoptime)
    hopchans.append(hopchan)
    hoptime += 14
    if(hopchan < 25):
        chandelt = 25
    elif(hopchan < 40):
        chandelt = 35
    elif(hopchan < 44):
        chandelt = 11
    elif(hopchan == 44):
        chandelt = 6
    else:
        chandelt = 10
    hopchan = (hopchan + chandelt) % len(chanlist)


plt.scatter(hoptimes, chanlist[hopchans], alpha=0.5, label="Predicted Hops")
plt.legend()
plt.xlabel("Time within cycle")
plt.ylabel("Frequency (MHz)")
plt.show()