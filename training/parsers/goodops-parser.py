#! /usr/bin/env python

from lab.parser import Parser


def read_good_operators(content, props):
    good_operators = [r for r in content.split("\n") if r]

    props["good_operators"] = good_operators
    props["num_good_operators"] =len(good_operators)



class GoodOperatorsParser(Parser):
    def __init__(self):
        Parser.__init__(self)
        self.add_function(read_good_operators, file="good_operators")


def main():
    parser = GoodOperatorsParser()
    parser.parse()


main()
