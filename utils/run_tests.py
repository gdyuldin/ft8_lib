#!/usr/bin/env python3
import argparse
import os, subprocess

scores = open("scores.csv", "w")

def parse(line):
    if line.startswith('cand:'):
        scores.write(line[6:] + '\n')
    fields = line.split(None, 5)
    if len(fields) < 5:
        return "", None
    snr = float(fields[1])
    time_offset = float(fields[2])
    freq = float(fields[3])
    msg = fields[5].split("  ")[0].strip()
    return msg, (snr, freq, time_offset)


def draw_results(expected, result):
    canvas = None


def main(wav_dir, is_ft4, live):
    wav_files = [os.path.join(wav_dir, f) for f in os.listdir(wav_dir)]
    wav_files = [f for f in wav_files if os.path.isfile(f) and os.path.splitext(f)[1] == '.wav']
    txt_files = [os.path.splitext(f)[0] + '.txt' for f in wav_files]

    n_extra = 0
    n_missed = 0
    n_total = 0
    expected_snrs = []
    result_snrs = []
    for wav_file, txt_file in zip(wav_files, txt_files):
        if not os.path.isfile(txt_file): continue
        print(wav_file)
        if live:
            cmd_args = ['./decode_ft8_live', wav_file]
        else:
            cmd_args = ['./decode_ft8', wav_file]
        if is_ft4:
            cmd_args.append('-ft4')
        result = subprocess.run(cmd_args, stdout=subprocess.PIPE)
        result = result.stdout.decode('utf-8').split('\n')
        res_dict = {}
        for r in result:
            k, items = parse(r)
            if k:
                res_dict[k] =items

        expected = open(txt_file).read().split('\n')
        exp_dict = {}
        for r in expected:
            k, items = parse(r)
            if k:
                exp_dict[k] = items

        extra_decodes = res_dict.keys() - exp_dict.keys()
        missed_decodes = exp_dict.keys() - res_dict.keys()
        print(len(result), '/', len(expected))
        if len(extra_decodes) > 0:
            print('Extra decodes: ', list(sorted(extra_decodes)))
        if len(missed_decodes) > 0:
            print('Missed decodes: ', list(sorted(missed_decodes)))
            for _ in missed_decodes:
                scores.write('0,1\n')

        for k in exp_dict.keys() & res_dict.keys():
            expected_snrs.append(exp_dict[k][0])
            result_snrs.append(res_dict[k][0])
        n_total += len(exp_dict)
        n_extra += len(extra_decodes)
        n_missed += len(missed_decodes)

        #break

    print('Total: %d, extra: %d (%.1f%%), missed: %d (%.1f%%)' %
            (n_total, n_extra, 100.0*n_extra/n_total, n_missed, 100.0*n_missed/n_total))
    recall = (n_total - n_missed) / float(n_total)

    diff = [(y - x) for x, y in zip(expected_snrs, result_snrs)]
    diff_mean = sum(diff) / len(diff)
    diff_std = (sum((x - diff_mean) ** 2 for x in diff) / len(diff)) ** 0.5
    print(f"SNR diff mean: {diff_mean:0.2f}, SNR diff std: {diff_std:0.2f}")
    print('Recall: %.1f%%' % (100*recall, ))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("wav_dir", help="Directory with wav files")
    parser.add_argument("--ft4", dest="is_ft4", action="store_true", default=False, help="Use FT4")
    parser.add_argument("--live", action="store_true", default=False, help="Use live decoder")
    args = parser.parse_args()
    main(**vars(args))


