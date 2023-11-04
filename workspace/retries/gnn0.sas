begin_version
3
end_version
begin_metric
0
end_metric
9
begin_variable
var0
-1
2
Atom clear(b2)
NegatedAtom clear(b2)
end_variable
begin_variable
var1
-1
2
Atom clear(b4)
NegatedAtom clear(b4)
end_variable
begin_variable
var2
-1
2
Atom arm-empty()
NegatedAtom arm-empty()
end_variable
begin_variable
var3
-1
5
Atom holding(b2)
Atom on(b2, b1)
Atom on(b2, b3)
Atom on(b2, b4)
Atom on-table(b2)
end_variable
begin_variable
var4
-1
5
Atom holding(b4)
Atom on(b4, b1)
Atom on(b4, b2)
Atom on(b4, b3)
Atom on-table(b4)
end_variable
begin_variable
var5
-1
5
Atom holding(b1)
Atom on(b1, b2)
Atom on(b1, b3)
Atom on(b1, b4)
Atom on-table(b1)
end_variable
begin_variable
var6
-1
5
Atom holding(b3)
Atom on(b3, b1)
Atom on(b3, b2)
Atom on(b3, b4)
Atom on-table(b3)
end_variable
begin_variable
var7
-1
2
Atom clear(b1)
NegatedAtom clear(b1)
end_variable
begin_variable
var8
-1
2
Atom clear(b3)
NegatedAtom clear(b3)
end_variable
5
begin_mutex_group
5
2 0
5 0
3 0
6 0
4 0
end_mutex_group
begin_mutex_group
5
7 0
5 0
3 1
6 1
4 1
end_mutex_group
begin_mutex_group
5
0 0
5 1
3 0
6 2
4 2
end_mutex_group
begin_mutex_group
5
8 0
5 2
3 2
6 0
4 3
end_mutex_group
begin_mutex_group
5
1 0
5 3
3 3
6 3
4 0
end_mutex_group
begin_state
1
1
0
4
4
3
2
0
0
end_state
begin_goal
6
3 4
4 4
5 1
6 3
7 0
8 0
end_goal
27
begin_operator
pickup b1
0
3
0 2 0 1
0 7 0 1
0 5 4 0
1
end_operator
begin_operator
pickup b2
0
3
0 2 0 1
0 0 0 1
0 3 4 0
1
end_operator
begin_operator
pickup b3
0
3
0 2 0 1
0 8 0 1
0 6 4 0
1
end_operator
begin_operator
pickup b4
0
3
0 2 0 1
0 1 0 1
0 4 4 0
1
end_operator
begin_operator
putdown b1
0
3
0 2 -1 0
0 7 -1 0
0 5 0 4
1
end_operator
begin_operator
putdown b2
0
3
0 2 -1 0
0 0 -1 0
0 3 0 4
1
end_operator
begin_operator
putdown b3
0
3
0 2 -1 0
0 8 -1 0
0 6 0 4
1
end_operator
begin_operator
putdown b4
0
3
0 2 -1 0
0 1 -1 0
0 4 0 4
1
end_operator
begin_operator
stack b1 b2
0
4
0 2 -1 0
0 7 -1 0
0 0 0 1
0 5 0 1
1
end_operator
begin_operator
stack b1 b3
0
4
0 2 -1 0
0 7 -1 0
0 8 0 1
0 5 0 2
1
end_operator
begin_operator
stack b2 b1
0
4
0 2 -1 0
0 7 0 1
0 0 -1 0
0 3 0 1
1
end_operator
begin_operator
stack b2 b3
0
4
0 2 -1 0
0 0 -1 0
0 8 0 1
0 3 0 2
1
end_operator
begin_operator
stack b2 b4
0
4
0 2 -1 0
0 0 -1 0
0 1 0 1
0 3 0 3
1
end_operator
begin_operator
stack b3 b1
0
4
0 2 -1 0
0 7 0 1
0 8 -1 0
0 6 0 1
1
end_operator
begin_operator
stack b3 b4
0
4
0 2 -1 0
0 8 -1 0
0 1 0 1
0 6 0 3
1
end_operator
begin_operator
stack b4 b1
0
4
0 2 -1 0
0 7 0 1
0 1 -1 0
0 4 0 1
1
end_operator
begin_operator
stack b4 b3
0
4
0 2 -1 0
0 8 0 1
0 1 -1 0
0 4 0 3
1
end_operator
begin_operator
unstack b1 b2
0
4
0 2 0 1
0 7 0 1
0 0 -1 0
0 5 1 0
1
end_operator
begin_operator
unstack b1 b3
0
4
0 2 0 1
0 7 0 1
0 8 -1 0
0 5 2 0
1
end_operator
begin_operator
unstack b1 b4
0
4
0 2 0 1
0 7 0 1
0 1 -1 0
0 5 3 0
1
end_operator
begin_operator
unstack b2 b1
0
4
0 2 0 1
0 7 -1 0
0 0 0 1
0 3 1 0
1
end_operator
begin_operator
unstack b2 b3
0
4
0 2 0 1
0 0 0 1
0 8 -1 0
0 3 2 0
1
end_operator
begin_operator
unstack b3 b1
0
4
0 2 0 1
0 7 -1 0
0 8 0 1
0 6 1 0
1
end_operator
begin_operator
unstack b3 b2
0
4
0 2 0 1
0 0 -1 0
0 8 0 1
0 6 2 0
1
end_operator
begin_operator
unstack b3 b4
0
4
0 2 0 1
0 8 0 1
0 1 -1 0
0 6 3 0
1
end_operator
begin_operator
unstack b4 b1
0
4
0 2 0 1
0 7 -1 0
0 1 0 1
0 4 1 0
1
end_operator
begin_operator
unstack b4 b3
0
4
0 2 0 1
0 8 -1 0
0 1 0 1
0 4 3 0
1
end_operator
0
