For use the operator priority planner use the following command:

```
python3 plan.py data/blocksworld.dk benchmarks/blocksworld/domain.pddl benchmarks/blocksworld/training/easy/p30.pddl plan.txt
```

At the time to execute the plan.py file, it will create the file `action_probabilities.txt` inside the workspace folder.

you can also use the fast-downward command line 

```
python3 fast-downward.py benchmarks/blocksworld/domain.pddl benchmarks/blocksworld/training/easy/p01.pddl --search "eager_greedy([operator_priorities(priority=path())])"
```

priority could be path() or instant()