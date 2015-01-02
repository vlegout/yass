#!/usr/bin/env python

import json, random

def gcd(a, b):
    while b:
        a, b = b, a % b
    return a

def lcm(a, b):
    return a * b // gcd(a, b)

def generate_utilizations(n_tasks, utilization):

    sum_u = utilization
    total = 0.0
    util = []

    for i in range(n_tasks):

        if i != n_tasks - 1:
            next_sum = sum_u * pow(random.random(), 1 / (n_tasks - (float(i + 1))))
            u = sum_u - next_sum
        else:
            u = utilization - total

        total += u
        sum_u = next_sum

        util.append(u)

    return util

def utilization_is_valid(util, utilization):
    sum_u = 0.0

    if util == []:
        return

    for u in util:
        if u < 0.01 or u > 0.99:
            return 0

        sum_u += u

    if sum_u != utilization:
        return 0

    return 1

def generate_tasks(utilization, n_tasks, hmax, mc_ratio, output, n_vms):

    done = 0

    random.seed()

    utilization = float(utilization) / 100

    if utilization <= 0 or utilization > 32:
        return -0

    while done != 1:

        util = []
        while not utilization_is_valid(util, utilization):
            util = generate_utilizations(n_tasks, utilization)

        h = 0
        hmin = 64

        if hmax == 0:
            hmax = 2048

        final_u = 0.0
        n = 0

        vm_sched = 0

        while n != 10000 and (vm_sched == 0 or h < hmin or h > hmax or final_u != sum(util)):
            h = 1
            final_u = 0.0

            wcet = []
            period = []
            vm = []

            for u in util:
                w = 0
                p = 1

                while w <= 0 or w >= p:
                    p = 10 * random.randint(2,10)
                    w = int(round(float(p) * u))

                h = lcm(h,p)

                period.append(p)
                wcet.append(w)
                vm.append(random.randint(0, n_vms - 1))

                final_u += float(float(w) / float(p))

            utilization_vm = []

            for util_vm in range(0, n_vms):
                utilization_vm.append(0)

            for t in range(0, n_tasks):
                utilization_vm[vm[t]] += float(wcet[t]) / float(period[t])

            vm_sched = 1

            for util_vm in utilization_vm:
                if util_vm > 1:
                    vm_sched = 0

            vm_list = []

            for v in vm:
                if v not in vm_list:
                    vm_list.append(v)

            if len(vm_list) != n_vms:
                vm_sched = 0

            n += 1

        if n != 10000:
            done = 1

    data = {"tasks", 1}

    list_t = []
    ratio = 0.0

    for i in range(n_tasks):

        ratio += 1 / float(n_tasks)

        if ratio > mc_ratio:
            criticality = 0
	else:
            criticality = 1

        dict_t = {
            'id': i + 1,
            'wcet': wcet.pop() * 10,
            'period': period.pop() * 10,
            'criticality': criticality,
            'vm': vm.pop()
        }

        list_t.append(dict_t)

    dict_t = {'tasks': list_t}

    with open(output, 'w') as outfile:
        json.dump(dict_t, outfile, sort_keys=True, indent=4, separators=(',', ': '))
