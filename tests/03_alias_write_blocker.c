// This store should NOT be hoisted
// LISHE conservatively avoids hoisting when aliasing is possible
void alias_block(int *p, int *q, int n, int v)
{
    if (n <= 0)
        return;
    for (int i = 0; i < n; ++i)
    {
        *q = i; // may alias with *p
        *p = v; // cannot be hoisted
    }
}
