"""Offline exact-rational branch-equation reference for Dc.ExactRationalBridge.

Run with Python's standard library. No Eigen, conductance stamping, or runtime
build/test dependency. Unknowns are V1,V2,V3 and one current per listed branch.
"""
from fractions import Fraction as F

branches = [
    ("V", 1, 0, 10), ("R", 1, 2, 1000), ("R", 2, 0, 1000),
    ("R", 1, 3, 330), ("R", 2, 3, 470), ("I", 3, 0, F(3, 1000)),
    ("V", 3, 2, 2),
]
nv = 3
size = nv + len(branches)
a = [[F(0) for _ in range(size + 1)] for _ in range(size)]
for i, (kind, p, n, value) in enumerate(branches):
    row = nv + i
    if p:
        a[p - 1][row] += 1
    if n:
        a[n - 1][row] -= 1
    if kind == "I":
        a[row][row], a[row][-1] = F(1), F(value)
    else:
        if p:
            a[row][p - 1] += 1
        if n:
            a[row][n - 1] -= 1
        if kind == "R":
            a[row][row] = -F(value)
        else:
            a[row][-1] = F(value)
for j in range(size):
    pivot = next(i for i in range(j, size) if a[i][j])
    a[j], a[pivot] = a[pivot], a[j]
    scale = a[j][j]
    a[j] = [x / scale for x in a[j]]
    for i in range(size):
        if i != j:
            scale = a[i][j]
            a[i] = [x - scale * y for x, y in zip(a[i], a[j])]
print("V1,V2,V3:", [str(a[i][-1]) for i in range(nv)])
print("V-source currents:", [str(a[nv + i][-1]) for i, b in enumerate(branches) if b[0] == "V"])
