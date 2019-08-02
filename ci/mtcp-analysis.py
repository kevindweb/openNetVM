import sys
import os
import json
import re

"""
get median value of tx pps
"""
def median(array):
    array = sorted(array)
    half, odd = divmod(len(array), 2)
    if odd:
        return array[half]
    else:
        return (array[half - 1] + array[half]) / 2.0

if(len(sys.argv) != 5):
    print("ERROR: Invalid arguments.")
    sys.exit(1)

STATS_FILE = sys.argv[1]
STATS_NODE_NAME = sys.argv[2]
OUT_FILE = sys.argv[3]
AVG_SPEED = float(sys.argv[4])

with open(STATS_FILE, "r") as f:
    contents = f.read()

#pattern = re.compile("\[CPU 0\] dpdk0 flows:.*RX:\s*([0-9]*).*")
pattern = re.compile("Time per request:\s*([\.\d]+) \[ms\] \(mean, across.*")
#data = [int(x) for x in pattern.findall(contents) if int(x) != 0]
#median_speed = median(data)
median_speed = float(pattern.findall(contents)[0])

results = {}
results['node'] = STATS_NODE_NAME
results['speed'] = median_speed

performance_rating = round((median_speed / AVG_SPEED) * 100, 2)
if (performance_rating < 97):
    results['pass_performance_check'] = False
else:    
    results['pass_performance_check'] = True
results['performance_rating'] = performance_rating
results['results_from'] = "[Results from %s]" % (STATS_NODE_NAME)
results['summary'] = "\n - Time (ms) per request for mTCP and Apache Benchmark: %f\n Performance rating - %.2f%% (compared to %f average)" % (median_speed, performance_rating, AVG_SPEED)

with open(OUT_FILE, 'w') as outfile:
    json.dump(results, outfile)
