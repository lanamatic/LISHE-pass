// Demonstrates redundant store elimination
// Two identical stores in the loop; LISHE removes the second one
void redundant(int *p, int n, int v)
{
    if (n <= 0)
        return;
    for (int i = 0; i < n; ++i)
    {
        *p = v; // first store
        *p = v; // redundant store -> eliminated
    }
}
