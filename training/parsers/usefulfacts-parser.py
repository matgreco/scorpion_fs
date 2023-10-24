#! /usr/bin/env python

from lab.parser import Parser


def read_relaxed_facts(content, props):
    relaxed_facts = [r for r in content.split("\n") if r]
    props["relaxed_facts"] = relaxed_facts
    props["num_relaxed_facts"] =len(relaxed_facts)


PATTERNS = [
    ("types", r"Total number of types: (\d+)", int),
    ("predicates", r"Total number of predicates: (\d+)", int),
    ("objects", r"Total number of objects: (\d+)", int),
    ("initial_state_atoms", r"Total number of atoms in the initial state: (\d+)", int),
    ("initial_state_atoms", r"Total number of atoms in the initial state: (\d+)", int),
    ("fluent_goal_atoms", r"Total number of fluent atoms in the goal state: (\d+)", int),
    ("action_schemas", r"Total number of action_schemas: (\d+)", int),
]

class UsefulFactsParser(Parser):
    def __init__(self):
        Parser.__init__(self)

        for name, pattern, typ in PATTERNS:
            self.add_pattern(name, pattern, type=typ)

        self.add_function(read_relaxed_facts, file="relaxed_facts")


def main():
    parser = UsefulFactsParser()
    parser.parse()


main()
