#!/usr/bin/env python

import json, re, os, sys

from gen_tasks import generate_tasks, lcm

import matplotlib.pyplot as plt

from subprocess import Popen, call

s = {
    'G-EDF':'../schedulers/.libs/gedf.so',
    'P-EDF':'../schedulers/.libs/pedf.so',
    'LPDPM':'../schedulers/.libs/lpdpm1.so',
    'LPDPM2':'../schedulers/.libs/lpdpm2.so',
    'LPDPM-MC(0.2)':'../schedulers/.libs/lpdpmmc1.so',
    'LPDPM-MC(0.4)':'../schedulers/.libs/lpdpmmc2.so',
    'LPDPM-MC(0.6)':'../schedulers/.libs/lpdpmmc3.so',
    'LPDPM-MC(0.8)':'../schedulers/.libs/lpdpmmc4.so',
    'LPDPM-MC(0.9)':'../schedulers/.libs/lpdpmmc5.so',
    'IZL':'../schedulers/.libs/izl.so',
    'RUN':'../schedulers/.libs/run.so',
    'U-EDF':'../schedulers/.libs/uedf.so',
    'EDF':'../schedulers/.libs/edf.so',
    'LLF':'../schedulers/.libs/llf.so',
    'CSF':'../schedulers/.libs/csf.so',
    'CSFD':'../schedulers/.libs/csfd.so',
    'CSF-PROB-50':'../schedulers/.libs/csfp50.so',
    'CSFD-PROB-50':'../schedulers/.libs/csfdp50.so',
    'CSF-PROB-80':'../schedulers/.libs/csfp80.so',
    'CSFD-PROB-80':'../schedulers/.libs/csfdp80.so',
    'CSF-N-50':'../schedulers/.libs/csfn50.so',
    'CSF-N-80':'../schedulers/.libs/csfn80.so',
}

def plot(utilizations, final, n_cpus, schedulers, ylabel, output):
    plt.figure()

    # plt.ylabel(ylabel, size='large')
    plt.xlabel('Global utilization', size='large')

    # plt.xlim(n_cpus - 1, n_cpus)

    max_y = 0

    symbol = ['s-', 'p-', '*-', 'h-', 'o-', 'v-', 'v-', '-o']

    end = []

    for i in range(len(schedulers)):
        r = []

        utils = list(utilizations)

        for u in range(len(utilizations)):
            if len(final[i][u]) > 0:
                r.append(sum(final[i][u]) / float(len(final[i][u])))
            else:
                utils.remove(utilizations[u])

        if max(r) > max_y:
            max_y = max(r)

        if output != 'deadlines':
            plt.ylim(0, max_y + 10)

        plt.plot(utils, r, symbol[i], label=schedulers[i], markersize=8)

        end.append(r)

    plt.legend(loc='best')

    output_pdf = output + '.pdf'
    output_data = output + '.data'

    plt.savefig(output_pdf, bbox_inches='tight')

    f = open(output_data, 'w')

    f.write('utilization ')

    for i in range(len(schedulers)):
        f.write(schedulers[i])
        if i != len(schedulers) - 1:
            f.write(' ')

    f.write('\n')

    for u in range(len(utilizations)):
        s = str(utilizations[u])
        s += ' '

        for i in range(len(schedulers)):
            s += str(end[i][u])

            if i != len(schedulers) - 1:
                s += ' '

        f.write(s)
        f.write('\n')

    f.close()

def plot_consumption(utilizations, final, n_sched, n_cpus, schedulers):
    plt.figure()

    # plt.ylabel('Mean consumption', size='large')
    plt.xlabel('Global utilization', size='large')

    plt.xlim(n_cpus - 1, n_cpus)

    max_y = 0

    symbol = ['s-', 'p-', '*-', 'h-', 'o-', 'v-', 'v-', '-o']

    if 'LPDPM' not in schedulers:
        return

    r = []

    for i in range(n_sched):
        r.append([])

        utils = list(utilizations)

        for u in range(len(utilizations)):
            if len(final[i][u]) > 0:
                r[i].append(sum(final[i][u]) / len(final[i][u]))
            else:
                utils.remove(utilizations[u])

        if max(r[i]) > max_y:
            max_y = max(r[i])

    lpdpm = schedulers.index('LPDPM')

    r_lpdpm = []

    for i in range(n_sched):
        r_lpdpm.append([])

        if schedulers[i] == 'LPDPM':
            continue

        for u in range(len(utilizations)):
            percent = (float(r[i][u]) - float(r[lpdpm][u])) / float(r[lpdpm][u])
            r_lpdpm[i].append(1 + percent)

        plt.plot(utils, r_lpdpm[i], symbol[i], label=schedulers[i], markersize=8)

    # plt.legend(loc='right', bbox_to_anchor = (1, 0.3))
    plt.legend(loc='best')

    plt.savefig('consumption_lpdpm.pdf', bbox_inches='tight')

    f = open('consumption_lpdpm.data', 'w')

    f.write('utilization ')

    for i in range(n_sched - 1):
        f.write(schedulers[i])
        if i != n_sched - 2:
            f.write(' ')

    f.write('\n')

    for u in range(len(utilizations)):
        s = str(utilizations[u])
        s += ' '

        for i in range(n_sched - 1):
            percent = (float(r[i][u]) - float(r[lpdpm][u])) / float(r[lpdpm][u])
            r_lpdpm[i].append(1 + percent)

            s += str(1 + percent)

            if i != n_sched - 2:
                s += ' '

        f.write(s)
        f.write('\n')

    f.close()

def plot_bars(schedulers):

    plt.figure()

    # plt.ylabel('Number of occurences', size='large')
    plt.xlabel('Idle period Length (ms)', size='large')

    w = 50
    n_intervals = 5
    interval = 400

    colors = ['r', 'y', 'b', 'g', 'c', 'm', 'k', 'r']

    total = []

    for i in range(len(schedulers)):
        f = open('sched/{0}'.format(i), 'r')

        total.append([])

        f.seek(0)
        tmp = f.readlines()
        f.close

        tmp = [t.rstrip('\n') for t in tmp]

        for t in tmp:
            tmp2 = re.split(' ', t)

            tmp2.remove('')

            for t2 in tmp2:
                total[i].append(int(t2))

    for i in range(len(schedulers)):

        data = []
        labels = []
        x = []

        for j in range(n_intervals):
            d_min = j * interval
            d_max = (j + 1) * interval

            if j == n_intervals - 1:
                d_max = sys.maxint

            data.append(0)

            for t in total[i]:
                if t >= d_min and t < d_max:
                    data[j] += 1

            x.append(j * interval + i * w)

        plt.bar(x, data, width=w, label=schedulers[i], color=colors[i], log=True)

    x = []
    x_labels = []

    for i in range(n_intervals):
        x.append(i * interval + (len(schedulers) * w) / 2)

        if i != n_intervals - 1:
            x_labels.append('{0} - {1}'.format((i * interval) / 10, ((i + 1) * interval) / 10))
        else:
            x_labels.append('{0} - inf'.format((i * interval) / 10))

    plt.xticks(x, x_labels, rotation=14)

    # Increase xlabel height, see http://stackoverflow.com/a/6776578
    # plt.gcf().subplots_adjust(bottom=0.14)

    plt.legend(loc='best')

    plt.savefig('hist.pdf', bbox_inches='tight')

def plot_usage(schedulers):

    plt.figure()

    schedulers.reverse()

    # plt.xlabel('Usage of low-power states', size='large')

    n_states = 3
    states_label = ['Sleep', 'Stop', 'Standby']

    bar_w = 2
    state_w = 10

    colors = ['r', 'y', 'b', 'g', 'c', 'm', 'k', 'r']

    total = []

    for i in range(len(schedulers)):
        f = open('usage/{0}'.format(i), 'r')

        total.append([])

        for j in range(n_states):
            total[i].append(0)

        f.seek(0)
        tmp = f.readlines()
        f.close

        tmp = [t.rstrip('\n') for t in tmp]

        for t in tmp:
            tmp2 = re.split(' ', t)

            tmp2.remove('')

            for j in range(n_states):
                total[i][j] = total[i][j] + int(tmp2[j])

    for i in range(len(schedulers)):

        x = []

        for j in range(n_states):
            x.append(j * state_w + i * bar_w)

        plt.bar(x, total[i], width=bar_w, label=schedulers[i], color=colors[i], log=True)

    x = []

    for i in range(n_states):
        x.append(i * state_w + state_w / 2)

    plt.xticks(x, states_label)

    plt.legend(loc='best')

    plt.savefig('usage.pdf', bbox_inches='tight')

def collect_results(utilizations, schedulers, n_tasksets):
    ctx = []
    idle = []
    consumption = []
    deadlines = []

    h = []

    n_sched = len(schedulers)

    schedulers.reverse()

    for i in range(n_sched):
        ctx.append([])
        idle.append([])
        consumption.append([])
        deadlines.append([])

    for k, u in enumerate(utilizations):

        h.append([])

        for s in range(n_sched):
            ctx[s].append([])
            idle[s].append([])
            consumption[s].append([])
            deadlines[s].append([])

        for i in range(n_tasksets):
            f = open('out/{0}.{1}.txt'.format(u,i), 'r')

            f.seek(0)
            tmp = f.readlines()
            f.close

            c = []

            if tmp == []:
                continue

            for s in range(n_sched):
                tmp2 = tmp.pop()
                tmp2 = re.split(' ', tmp2)
                tmp2[len(tmp2) - 1] = tmp2[len(tmp2) - 1].rstrip('\n')

                idle[s][len(idle[s]) - 1].append(float(tmp2[0]))
                ctx[s][len(ctx[s]) - 1].append(float(tmp2[1]))
                c.append(float(tmp2[2]))
                deadlines[s][len(deadlines[s]) - 1].append(float(tmp2[3]))

            hyperperiod = tmp.pop()
            hyperperiod = float(hyperperiod.rstrip('\n'))

            for s in range(n_sched):
                consumption[s][len(consumption[s]) - 1].append(c[s])
                # consumption[s][len(consumption[s]) - 1].append(c[s] - 2 * hyperperiod * u / 100)

    return idle, ctx, consumption, deadlines

def get_stat(utilizations, schedulers, n_tasksets):
    stat = []
    stat.append([])

    if 'LPDPM' not in schedulers:
        return

    schedulers.reverse()

    index_lpdpm = schedulers.index('LPDPM')

    schedulers.reverse()

    for u in utilizations:

        stat[0].append([])

        for i in range(n_tasksets):
            try:
                f = open('stats/{0}.{1}.txt.{2}'.format(u ,i, index_lpdpm), 'r')

                f.seek(0)

                tmp = f.readlines()
                tmp = tmp.pop()
                tmp = int(tmp)

                if tmp == 2:
                    stat[0][len(stat[0]) - 1].append(1)
                else:
                    stat[0][len(stat[0]) - 1].append(0)

                f.close
            except IOError:
                stat[0][len(stat[0]) - 1].append(0)

    return stat

def plot_stat(utilizations, final, n_cpus):
    r = []

    plt.figure()
    plt.xlabel('Global utilization', size='large')

    utils = list(utilizations)

    for u in range(len(utilizations)):
        if len(final[0][u]) > 0:
            r.append(sum(final[0][u]) / float(len(final[0][u])))
        else:
            utils.remove(utilizations[u])

    plt.xlim(n_cpus - 1, n_cpus)
    plt.ylim(0, 1.1)
    plt.plot(utils, r, 's-', label='LPDPM', markersize=8)

    plt.savefig('optimal.pdf', bbox_inches='tight')

def create_tasks(utilizations, n_tasksets, n_tasks, h_max, mc_ratio, n_vms):

    call(["rm", "-rf", "tmp"])
    call(["mkdir", "tmp"])

    for u in utilizations:
        for n in range(n_tasksets):
            generate_tasks(u, n_tasks, h_max, mc_ratio, 'tmp/{0}.{1}.txt'.format(u,n), n_vms)

def launch(utilizations, online, n_tasksets, n_tasks, n_cpus, n_hyperperiods, n_jobs, schedulers):

    call(["rm", "-rf", "out"])
    call(["mkdir", "out"])

    call(["rm", "-rf", "sched"])
    call(["mkdir", "sched"])

    call(["rm", "-rf", "usage"])
    call(["mkdir", "usage"])

    call(["rm", "-rf", "stats"])
    call(["mkdir", "stats"])

    call(["rm", "-rf", "log"])
    call(["mkdir", "log"])

    for u in utilizations:

        p = []

        for n in range(n_tasksets):

            command = ['../src/yass', '-c' ,'../processors/generic', '--tests', '--debug', '-v',
                       '-n', str(n_cpus),
                       '-d', 'tmp/{0}.{1}.txt'.format(u,n),
                       '-h', str(n_hyperperiods),
                       '--tests-output', '{0}.{1}.txt'.format(u,n)]

            for i in range(len(schedulers)):
                command.append('-s')
                command.append(schedulers[i])

            if online == 1:
                command.append('--online')

            f = open('log/{0}.{1}.txt'.format(u,n), 'w')

            p.append(Popen(command,stdout=f,stderr=f))

            if n != 0 and (n + 1) % n_jobs == 0:
                while len(p) > 0:
                    Popen.wait(p[0])
                    p.pop(0)

            f.close

        while len(p) > 0:
            Popen.wait(p[0])
            p.pop(0)

def main():

    config = json.load(open("config.json"))

    start = config['u_min']
    end = config['u_max']
    inc = config['u_inc']

    utilizations = range(start, end + 1, inc)

    h_max = config['hyperperiod']
    n_tasksets = config['n_tasksets']
    online = config['online']
    n_tasks = config['n_tasks']
    n_cpus = config['n_cpus']
    gen_tasks = config['gen_tasks']
    mc_ratio = config['mc_ratio']
    n_hyperperiods = config['n_hyperperiods']
    n_jobs = config['n_jobs']
    n_vms = config['n_vms']

    schedulers = []

    if 'LPDPM' in config['schedulers'] and config['schedulers'].index("LPDPM") != len(config['schedulers']) -1:
        print "Error: LPDPM must come last"
        sys.exit(1)

    for sched in config['schedulers']:
        schedulers.append(sched)

        if sched not in s:
            print "Unknown scheduler: " + sched
            sys.exit(1)

    n_sched = len(schedulers)

    if online != 0 and online != 1:
        print "ONLINE should either be 0 or 1"
        sys.exit(1)

    # if start / 100 < n_cpus - 1 or end > n_cpus * 100 or start > end:
    #     print "The number of processors and the utilizations do not match"
    #     sys.exit(1)

    list_schedulers = []

    for i in range(n_sched):
        list_schedulers.append(s.get(schedulers[i]))

    if gen_tasks == 1:
        create_tasks(utilizations, n_tasksets, n_tasks, h_max, mc_ratio, n_vms)

    launch(utilizations, online, n_tasksets, n_tasks, n_cpus, n_hyperperiods, n_jobs, list_schedulers)

    idle, ctx, consumption, deadlines = collect_results(utilizations, schedulers, n_tasksets)
    stat = get_stat(utilizations, schedulers, n_tasksets)

    utilizations = [float(u) / 100 for u in utilizations]

    plot(utilizations, idle, n_cpus, schedulers, 'Mean number of idle periods', 'idle')
    plot(utilizations, ctx, n_cpus, schedulers, 'Mean number of context switches', 'ctx')
    plot(utilizations, consumption, n_cpus, schedulers, 'Mean consumption', 'consumption')
    plot(utilizations, deadlines, n_cpus, schedulers, 'Mean number of deadline misses', 'deadlines')

    if 'LPDPM' in schedulers:
        plot_stat(utilizations, stat, n_cpus)
        plot_consumption(utilizations, consumption, n_sched, n_cpus, schedulers)

    plot_bars(schedulers)

    plot_usage(schedulers)

if __name__ == "__main__":
    main()
