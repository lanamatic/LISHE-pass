// Pointer depends on loop index, so store is NOT loop-invariant
// LISHE cannot hoist it
void ptr_varies(int *base, int n, int v)
{
    if (n <= 0)
        return;
    for (int i = 0; i < n; ++i)
    {
        base[i] = v; // pointer changes each iteration -> cannot hoist
    }
}
