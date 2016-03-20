#include "afio_pch.hpp"

int main(void)
{
    //[barrier_example
    // Assume that groups is 10,000 items long with item.first being randomly
    // between 1 and 500. This example is adapted from the barrier() unit test.
    //
    // What we're going to do is this: for each item in groups, schedule item.first
    // parallel ops and a barrier which completes only when the last of that
    // parallel group completes. Chain the next group to only execute after the
    // preceding group's barrier completes. Repeat until all groups have been executed.
    std::shared_ptr<boost::afio::dispatcher> dispatcher=
        boost::afio::make_dispatcher().get();
    std::vector<std::pair<size_t, int>> groups;
    boost::afio::atomic<size_t> callcount[10000];
    memset(&callcount, 0, sizeof(callcount));
    
    // This lambda is what each parallel op in each group will do: increment an atomic
    // for that group.
    auto inccount = [](boost::afio::atomic<size_t> *count){ (*count)++; };
    
    // This lambda is called after each barrier completes, and it checks that exactly
    // the right number of inccount lambdas were executed.
    auto verifybarrier = [](boost::afio::atomic<size_t> *count, size_t shouldbe)
    {
        if (*count != shouldbe)
            throw std::runtime_error("Count was not what it should have been!");
        return true;
    };
    
    // For each group, dispatch ops and a barrier for them
    boost::afio::future<> next;
    bool isfirst = true;
    for(auto &run : groups)
    {
        // Create a vector of run.first size of bound inccount lambdas
        // This will be the batch issued for this group
        std::vector<std::function<void()>> thisgroupcalls(run.first, std::bind(inccount, &callcount[run.second]));
        std::vector<boost::afio::future<>> thisgroupcallops;
        // If this is the first item, schedule without precondition
        if (isfirst)
        {
            thisgroupcallops = dispatcher->call(thisgroupcalls);
            isfirst = false;
        }
        else
        {
            // Create a vector of run.first size of preconditions exactly
            // matching the number in this batch. Note that the precondition
            // for all of these is the preceding verify op
            std::vector<boost::afio::future<>> dependency(run.first, next);
            thisgroupcallops = dispatcher->call(dependency, thisgroupcalls);
        }
        // barrier() is very easy: its number of output ops exactly matches its input
        // but none of the output will complete until the last of the input completes
        auto thisgroupbarriered = dispatcher->barrier(thisgroupcallops);
        // Schedule a call of the verify lambda once barrier completes. Here we choose
        // the first item of the barrier's return, but in truth any of them are good.
        auto verify = dispatcher->call(thisgroupbarriered.front(), std::function<bool()>(std::bind(verifybarrier, &callcount[run.second], run.first)));
        // Set the dependency for the next batch to be the just scheduled verify op
        next = verify;
    }
    // next was the last op scheduled, so waiting on it waits on everything
    when_all_p(next).wait();
    //]
}
