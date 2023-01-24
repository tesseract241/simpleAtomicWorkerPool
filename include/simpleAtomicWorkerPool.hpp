/// @file simpleAtomicWorkerPool.hpp
/*!
 * @brief Simple implementation of a worker pool backed by an array of jobs, using atomics and futexes as syncronization primitives
 */
/// @example example.cpp
#include "lockless_sleep_and_wake.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
/*! @brief Templated array holding the jobs and syncronization primitives, works as a FIFO queue for all intents and purposes
 *
 * The intended workflow for this class is to be used by a single dispatcher thread, which enqueues all the jobs, which then
 * starts the working threads and goes to sleep until they're finished. After the queue is done and the working threads
 * have gone to sleep, the dispatcher thread can enqueue new jobs into the array.
 * @tparam J The type of the jobs that the user wants to execute
 */
template<typename J>
class AtomicArray{
    public:
        /*! @brief Constructor for AtomicArray
         *  @param[in] size The starting size of the array
         */
        AtomicArray(int size){
            this->size = size;
            backingArray = new J[size];
            tailCursor = 0;
            headCursor.store(0);
            worker_start = 0;
            worker_end = 0;
            dispatcher_wake = 0;
        }
        ///@brief Destructor for AtomicArray, frees the dynamically allocated backing array
        ~AtomicArray(){
            delete[] backingArray;
        }
        /*! @brief Used to add jobs to the array
         *  @param[in]  element The job to add to the array at the bottom of the queue
         *  @return             A pointer to the job in the array
         *  @note               If the backing array is full, it will be reallocated and doubled in size
         * */
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
        /*! @brief Fetches the first job in the queue
         *  @return The first job in the queue, or nullptr if the queue is empty
         */
        J* fetch(){
            int index = headCursor.fetch_add(1);
            if(index>=tailCursor) return nullptr;
            return backingArray + index;
        }
        /// @brief Resets the internal counters that keep track of how full the array is, effectively treating it as empty
        void emptyOut(){
            tailCursor = 0;
            headCursor.store(0);
        }
        std::atomic_uint32_t worker_start;      ///< The atomic used as a syncronization primitive to tell the workers to wake up or go to sleep
        std::atomic_uint32_t worker_end;        ///< The atomic used as a syncronization primitive to tell the workers whether to return and become joinable
        std::atomic_uint32_t dispatcher_wake;   ///< The atomic used as a syncronization primitive to tell the dispatcher to wake up or go to sleep
    private:
        J *backingArray;                        ///< The memory backing the AtomicArray
        int tailCursor;                         ///< Internal counter to keep track of how full is the array 
        std::atomic_int headCursor;             ///< Internal counter to find the first job in the queue
        int size;                               ///< Current size of the allocated memory
};

/*! @brief Wrapper function for the working thread function that takes care of all the syncronization, sleeping and waking up
 * @tparam      J               The type of the jobs the user wants to execute 
 * @param[in]   atomicArray     The array providing the memory and syncronization primitives for the job queue
 * @param[in]   worker          The actual function that does the job, provided by the user, gets called on each job in the queue  
 */
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
/*! @brief Allocates and initializes the worker threads
 * @tparam      J               The type of the jobs the user wants to execute 
 * @param[in]   atomicArray     The array providing the memory and syncronization primitives for the job queue
 * @param[in]   worker          The actual function that does the job, provided by the user, gets called on each job in the queue  
 * @param[in]   threadNumber    The number of threads to spawn. Defaults to the number of cores on your machine, and won't exceed it even if you provide a number greater than it.
 * @return                      A pointer to the allocated threads
 */
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

/*! @brief Starts the worker threads and won't return until they're done. Resets the atomicArray to be reusable on exit.
 * @tparam      J               The type of the jobs the user wants to execute 
 * @param[in]   atomicArray     The array providing the memory and syncronization primitives for the job queue
 */
template<typename J> 
void dispatchJobs(AtomicArray<J> &atomicArray){
    wake_all(atomicArray.worker_start);
    sleep(atomicArray.dispatcher_wake);
    atomicArray.worker_start.store(0);
    atomicArray.emptyOut();
}

/*! @brief Tells the worker threads to stop, waits on them to become joinable and then frees their allocated memory
 * @tparam      J               The type of the jobs the user wants to execute 
 * @param[in]   atomicArray     The array providing the memory and syncronization primitives for the job queue
 * @param[in]   threads         The allocated threads to stop and free
 * @param[in]   threadNumber    The number of threads to end. Defaults to the number of cores on your machine, and just like createThreads won't exceed it even if you provide a number greater than it.
 */
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
