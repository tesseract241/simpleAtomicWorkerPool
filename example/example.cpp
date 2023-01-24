
#include <mutex>
#include <random>
#include <thread>
#include <deque>
#include <chrono>
#include "simpleAtomicWorkerPool.hpp"

struct Job {
   int* data;
   struct{
       int param1;
       float param2;
       int param3;
   } params;
};

void worker(Job &job){
    std::random_device rd;  
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<> dis(100, 350);
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
}
int main (int argc, char *argv[])
{
    const int jobNumber = 32;
    const int reps      = 50;
    AtomicArray<Job> atomicArray(jobNumber);
    std::thread *threads = createThreads<Job>(atomicArray, worker);
    for(int i=0;i<reps;++i){
        for(int j=0;j<jobNumber;++j){
            Job job = Job();
            atomicArray.append(job);
        }
        dispatchJobs(atomicArray);
    }
    endThreads(atomicArray, threads);
    return 0;
}
