import sys
import subprocess
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd

OUTPUT_DIR = 'output/tcp-congestion-control/'
SEGMENT_SIZE = 536  # [bytes]

prefix = ('compare-cwnd' if len(sys.argv) == 1 else sys.argv[1])
print(prefix)

tcpTypeArray = ['TcpNewReno', 'TcpCubic', 'TcpLinuxReno',
                'TcpHighSpeed', 'TcpHybla', 'TcpWestwood', 'TcpVegas', 'TcpScalable',
                'TcpVeno', 'TcpBic', 'TcpYeah', 'TcpIllinois', 'TcpHtcp',
                'TcpLedbat', 'TcpLp', 'TcpDctcp', 'TcpBbr']

N = len(tcpTypeArray)
W = 3
H = int((N + 1) / W)

plt.figure(figsize=(4 * W, 3 * H))

for i, tcpType in enumerate(tcpTypeArray):
    print(tcpType)

    res = subprocess.run(['./ns3', 'run', 'scratch/230104_TcpCongestionControl/main.cc', '--',
                          '--tcpType=' + tcpType, '--tracing', '--prefix=' + tcpType],
                         capture_output=True)
    print('ns3:', res.returncode)
    # print(res.stdout)
    # print(res.stderr)

    data_cwnd = pd.read_csv(OUTPUT_DIR + tcpType + '_cwnd_0_0.csv')
    data_ssth = pd.read_csv(OUTPUT_DIR + tcpType + '_ssth_0_0.csv')

    plt.subplot(H, W, i + 1)

    plt.plot(data_cwnd.time, data_cwnd.newValue / SEGMENT_SIZE, label='cwnd')
    plt.plot(data_ssth.time, data_ssth.newValue / SEGMENT_SIZE, linestyle='dashed', label='ssth')

    plt.ylim(0, np.max(data_cwnd.newValue / SEGMENT_SIZE) + 10)

    plt.xlabel('time[s]')
    plt.ylabel('segment')
    plt.title(tcpType)

    plt.legend()
    plt.grid()

plt.tight_layout()
plt.savefig(OUTPUT_DIR + prefix + '.png')
