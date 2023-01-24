#include <lockless_sleep_and_wake.hpp>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>

template<typename J>
class AtomicArray{
    public:
        AtomicArray(int size){
            this->size = size;
            backingArray = new J[size];
            tailCursor = 0;
            headCursor.store(0);
            worker_start = 0;
            worker_end = 0;
            dispatcher_wake = 0;
        }
        ~AtomicArray(){
            delete[] backingArray;
        }
        J* append(J element){
            int index = tailCursor;
            ++tailCursor;
            if(index==size){
                J* oldArray = backingArray;
                backingArray = new J[size*2];
                std::memcpy(backingArray, oldArray, size * sizeof(J));
                size = size*2;
                delete[] oldArray;
            }
            std::memcpy(backingArray + index, &element, sizeof(J));
            return backingArray + index;
        }
        J* fetch(){
            int index = headCursor.fetch_add(1);
            if(index>=tailCursor) return nullptr;
            return backingArray + index;
        }
        void emptyOut(){
            tailCursor = 0;
            headCursor.store(0);
        }
        std::atomic_uint32_t worker_start;
        std::atomic_uint32_t worker_end;
        std::atomic_uint32_t dispatcher_wake;
    private:
        J *backingArray;
        int tailCursor;
        std::atomic_int headCursor;
        int size;
};

template<typename J>
void threadFunction(AtomicArray<J> &atomicArray, void worker(J &job)){
    while(1){
        sleep(atomicArray.worker_start);
        if(atomicArray.worker_end.load()) return;
        while(1){
            J* job = atomicArray.fetch();
            if(!job){
                wake_all(atomicArray.dispatcher_wake);
                break;
            }
            worker(*job);
        }
    }
}

template<typename J>
std::thread* createThreads(AtomicArray<J> &atomicArray, void worker(J &job), int threadNumber=std::thread::hardware_concurrency()){
    atomicArray.worker_start.store(0);
    threadNumber = std::min(threadNumber, (int) std::thread::hardware_concurrency());
    if(!threadNumber) threadNumber = 1;
    std::thread *threads = new std::thread[threadNumber];
    for(int i=0;i<threadNumber;++i){
        threads[i] = std::thread(threadFunction<J>, std::ref(atomicArray), worker);
    }
    return threads;
}

template<typename J> 
void dispatchJobs(AtomicArray<J> &atomicArray){
    wake_all(atomicArray.worker_start);
    sleep(atomicArray.dispatcher_wake);
    atomicArray.worker_start.store(0);
    atomicArray.emptyOut();
}

template<typename J> 
void endThreads(AtomicArray<J> &atomicArray, std::thread *threads, int threadNumber=std::thread::hardware_concurrency()){
    threadNumber = std::min(threadNumber, (int) std::thread::hardware_concurrency());
    if(!threadNumber) threadNumber = 1;
    atomicArray.worker_end.store(1);
    wake_all(atomicArray.worker_start);
    for(int i=0;i<threadNumber;++i){
        threads[i].join();
        wake_all(atomicArray.worker_start);
    }
    delete[] threads;
}
