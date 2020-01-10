import re
import sys
import argparse
import numpy as np
from bisect import bisect_right
from random import randint
from math import log2, ceil
from os.path import join as path_join, exists as path_exists, sep as path_sep
from os import walk as files_walker
from collections import defaultdict
from typing import DefaultDict
import itertools
import matplotlib.pyplot as plt


def gen_guess_crack(estimations: [int], upper_bound=10 ** 20):
    guesses = [0]
    cracked = [0]
    estimations.sort()
    for m, n in itertools.groupby(estimations):
        if m <= upper_bound:
            guesses.append(m)
            cracked.append((cracked[-1]) + len(list(n)))
    return guesses[1:], cracked[1:]
    pass


def draw_gc_curve(guesses: [int], cracked: [float], label, save2file: str):
    plt.plot(guesses, cracked, label=label)
    plt.xscale("log")
    plt.grid(ls="--")
    plt.xlabel('Guesses')
    plt.ylabel('Cracked(%)')
    plt.legend(loc=2)
    plt.savefig(save2file)

    pass


class TransPCFGModel:
    __grammars: DefaultDict[str, float]
    __terminals: DefaultDict[str, DefaultDict[str, float]]

    def set_test_set(self, test_set):
        self.__test_set = test_set

    def set_upper_bound(self, upper_bound):
        self.__upper_bound = upper_bound

    def __init__(self, grammars, letter, digits, symbol, sample_size, test_set, upper_bound):
        self.__grammar_pattern = re.compile(r"(L+|D+|S+)")
        self.__password_pattern = re.compile(r"([a-zA-Z]+)|([0-9]+)|([\x21-\x2f\x3a-\x40\x5b-\x60\x7b-\x7e]+)")
        self.__terminals = defaultdict(lambda: defaultdict(float))
        for __l, items in letter.items():
            for item, prob in items.items():
                self.__terminals[f'L' * __l][item] = prob
        for __l, items in digits.items():
            for item, prob in items.items():
                self.__terminals[f'D' * __l][item] = prob
        for __l, items in symbol.items():
            for item, prob in items.items():
                self.__terminals[f'S' * __l][item] = prob
        self.__grammars = grammars
        self.__log_prob_of_samples = []
        self.__sample_guesses = []
        self.__sample_size = sample_size
        self.__test_set = test_set
        self.__upper_bound = upper_bound
        pass

    def __generate_one(self) -> (str, float):
        grammars_keys = list(self.__grammars.keys())
        grammar_idx = randint(0, len(grammars_keys) - 1)

        struct = grammars_keys[grammar_idx]
        guess = ""
        log_prob = -log2(self.__grammars.get(struct))
        for match in self.__grammar_pattern.finditer(struct):
            group = match.group()
            terminals = self.__terminals.get(group)
            terminals_keys = list(terminals.keys())
            terminal_idx = randint(0, len(terminals_keys) - 1)
            terminal = terminals_keys[terminal_idx]
            guess += terminal
            log_prob += -log2(terminals.get(terminal))
        return guess, log_prob

    def sample(self):
        for i in range(self.__sample_size):
            guess, log_prob = self.__generate_one()
            self.__sample_guesses.append(guess)
            self.__log_prob_of_samples.append(log_prob)
            if i % 5000 == 0:
                print(f"Progress: {i / self.__sample_size * 100: 5.2f}%", file=sys.stderr)

    def log_prob(self, pwd):
        log_prob = 0
        struct = ""
        for match in self.__password_pattern.finditer(pwd):
            l_part, d_part, s_part = match.groups()
            if l_part:
                group = "L" * len(l_part)
                terminal = l_part
            elif d_part:
                group = "D" * len(d_part)
                terminal = d_part
            elif s_part:
                group = "S" * len(s_part)
                terminal = s_part
            else:
                print(f"Unknown char found in {pwd}, make sure that "
                      "all chars in your file is printable ASCII", file=sys.stderr)
                return float("inf")
            if group in self.__terminals and terminal in self.__terminals.get(group):
                struct += group
                log_prob += -log2(self.__terminals.get(group).get(terminal))
            else:
                return float("inf")

        if struct in self.__grammars:
            log_prob += -log2(self.__grammars.get(struct))
            return log_prob
        else:
            return float('inf')

    def evaluate(self, test_set, save2file, label, upper_bound, guess_crack_file):
        if len(self.__log_prob_of_samples) != self.__sample_size:
            print("run sample function first.", file=sys.stderr)
            return
            # with open(self.tes)
        self.__upper_bound = upper_bound
        self.__test_set = test_set
        log_probs, samples = (list(t) for t in zip(*sorted(zip(self.__log_prob_of_samples, self.__sample_guesses))))
        log_probs = np.fromiter(log_probs, float)
        logn = log2(self.__sample_size)
        print(f"log_probs[0-100]: {log_probs[:100]}")
        print(f"sample[0-100]: {samples[:100]}")
        positions = (2 ** (log_probs - logn)).cumsum()
        print(f"positions[0~100]: {positions[:100]}")
        estimations = []
        test_set_len = 0
        with open(test_set, "r") as fin:
            for line in fin:
                line = line.strip("\r\n")
                log_prob = self.log_prob(line)
                idx = bisect_right(log_probs, log_prob)
                pos = positions[min(idx, len(positions) - 1)]
                if pos < 0.001:
                    print(f"{line}: {log_prob}")
                estimations.append(ceil(pos))
                test_set_len += 1
                if test_set_len % 10000 == 0:
                    print(f"progress: {test_set_len}")
        guesses, cracked = gen_guess_crack(estimations, upper_bound)
        cracked_ratio = [c / test_set_len * 100 for c in cracked]
        with open(guess_crack_file, "w") as fout:
            for g, c, r in zip(guesses, cracked, cracked_ratio):
                fout.write(f"{g} : {c} : {r:5.2f}\n")
            fout.flush()

        draw_gc_curve(guesses, cracked_ratio, label, save2file)

    @staticmethod
    def __load_grammar(abs_model_path: str) -> {str: float}:
        grammars = defaultdict(float)
        with open(path_join(abs_model_path, "model", "grammar", "structures.txt"), "r") as fin:
            for line in fin:
                line = line.strip("\r\n")
                try:
                    struct, prob = line.split("\t")
                    grammars[struct] = float(prob)
                except ValueError as e:
                    print(e, file=sys.stderr)
                    sys.exit(1)
        return grammars

    @staticmethod
    def __load_terminals(abs_model_path: str, terminal_type):
        terminal_length_regexp = re.compile(r"^(\d+)(?:.txt)$")
        terminals = defaultdict(lambda: defaultdict(float))
        for root, dirs, files in files_walker(path_join(abs_model_path, "model", terminal_type)):
            for file in files:
                number = int(terminal_length_regexp.match(file).groups()[0])
                with open(path_join(root, file), "r") as fin:
                    for line in fin:
                        terminal, prob = line.strip("\r\n").split("\t")
                        terminals[number][terminal] = float(prob)
        return terminals

    @staticmethod
    def build(abs_model_path: str, sample_size: int, abs_test_set: str, upper_bound: int):
        raw_letter = defaultdict(list)
        with open(path_join(abs_model_path, "dictionary.txt"), "r") as fin:
            for line in fin:
                line = line.strip("\r\n")
                raw_letter[len(line)].append(line)
        letter = defaultdict(lambda: defaultdict(float))
        for __l in raw_letter:
            for terminal in raw_letter.get(__l):
                letter[__l][terminal] = 1 / len(raw_letter.get(__l))
        grammar = TransPCFGModel.__load_grammar(abs_model_path)
        digits = TransPCFGModel.__load_terminals(abs_model_path, "digits")
        symbol = TransPCFGModel.__load_terminals(abs_model_path, "special")
        return TransPCFGModel(grammar, letter, digits, symbol, sample_size, abs_test_set, upper_bound)

    pass


def get_parent_dir(file: str):
    idx = file.rfind(path_sep)
    return file[:idx + 1]
    pass


if __name__ == '__main__':
    parser = argparse.ArgumentParser("Monte Carlo Simulation for TransPCFG")
    parser.add_argument("--trained-model", "-m", required=True, help='trained-model of TransPCFG')
    parser.add_argument("--test-set", "-t", required=True, help='password set to be cracked')
    parser.add_argument("--sample-size", "-s", required=False, default=10000, type=int,
                        help="Sample specified number of passwords, suggested number is 10000+")
    parser.add_argument("--upper-bound", "-u", required=False, default=10 ** 20, type=int,
                        help="Guess number larger than upper bound will be ignored")
    parser.add_argument("--guess-crack-file", "-f", required=True, type=str, help="save guess crack file")
    parser.add_argument("--save-gc-curve", "-c", required=True, type=str, help="save figure here")
    parser.add_argument("--prob-mode", dest="prob_mode", required=False, action="store_true", default=False,
                        help="type in password and return prob")
    args = parser.parse_args()
    if not path_exists(args.trained_model) or not path_exists(get_parent_dir(args.guess_crack_file)) \
            or not path_exists(get_parent_dir(args.save_gc_curve)) or not path_exists(args.test_set):
        sys.exit(1)
    transPCFGModel = TransPCFGModel.build(args.trained_model, args.sample_size, args.test_set, args.upper_bound)
    if args.prob_mode:
        while True:
            pwd = input("Type in password\n>>>")
            lp = transPCFGModel.log_prob(pwd)
            if lp > 10000:
                prob = float("inf")
            else:
                prob = 2 ** (-lp)
            print(f"{pwd}: {prob}, log prob: {lp}")
    else:
        transPCFGModel.sample()
        transPCFGModel.evaluate(args.test_set, args.save_gc_curve, args.trained_model, args.upper_bound,
                                args.guess_crack_file)
        pass
