# Instructions to use Operators priorities

Install the requirements.txt

For use the operator priority planner use the following command:

```
python3 plan.py data/blocksworld.dk benchmarks/blocksworld/domain.pddl benchmarks/blocksworld/training/easy/p30.pddl plan.txt 10000 10000 --priority "instant" --search "eager_greedy([ff(), opp])"
```

Check that there are no errors and the planner show the "actions probabilities"

```
ACTIONS probabilities
tensor([[1.0000],
        [0.9909],
        [0.4064],
        ...
```

The type of operator priority could be None, "instant", "path", "path_norm".