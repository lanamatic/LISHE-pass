// Loop contains a read before the store
// Still safe for hoisting because the value being stored is invariant
int guarded(int *p, int n, int v)
{
    int sum = 0;
    if (n <= 0)
        return 0;
    for (int i = 0; i < n; ++i)
    {
        sum += *p; // reading the memory
        *p = v;    // can still be hoisted
    }
    return sum;
}
