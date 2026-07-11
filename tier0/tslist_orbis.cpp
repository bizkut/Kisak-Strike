// Tier0's TSList entry points are developer stress tests, not runtime services.
// Keep their public probes linkable without constructing the test-only queues.
extern "C" bool RunTSListTests( int, int )
{
    return false;
}

extern "C" bool RunTSQueueTests( int, int )
{
    return false;
}
