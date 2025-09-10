// Volatile stores must not be moved
// Even if the value and pointer are invariant, the store has side-effect semantics
typedef struct
{
    int x;
} Reg;
void vol(Reg *r, int n, int v)
{
    if (n <= 0)
        return;
    for (int i = 0; i < n; ++i)
    {
        __atomic_store_n(&r->x, v, __ATOMIC_RELAXED); // volatile-like store
    }
}
