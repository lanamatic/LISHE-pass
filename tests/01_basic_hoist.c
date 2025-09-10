// Candidate for loop-invariant store hoisting
// *p = v; — both pointer and value are loop-invariant
void basic(int *p, int n, int v)
{
    if (n <= 0)
        return; // helps ScalarEvolution see that loop executes >= 1
    for (int i = 0; i < n; ++i)
    {
        *p = v; // should be hoisted out of the loop
    }
}
