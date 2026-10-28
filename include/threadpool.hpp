#pragma once 

#include <thread>
#include <functional>
#include <condition_variable>
#include <queue>
#include <future>


class Threadpool{
public:
    using Task = std::function<void()>;

    Threadpool(size_t thread_num = std::thread::hardware_concurrency());

    ~Threadpool();

    void enqueue(Task task);

    //Since this is not machine code but rather an abstraction we define it in hpp
    template <typename F>
    //The && means an universal reference aka it accepts both lvalues and rvalues
    //The -> means it is a trailing return type aka when the return type of the function depends on the parameters
    auto submit(F&& f) -> std::future<std::invoke_result_t<F>> {
        //Here invoke result will ask the compiler that the return type of thing F is
        using ReturnType = std::invoke_result_t<F>;

        //package has promise and future bundled in, since we pass an rvalue we need forward so that it doesnt treat is as an lvalue
        //since packaged_task cannot be copied we use a shared pointer so that we can copy it
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<F>(f)); 

        //We get out future from the package
        std::future<ReturnType> result = task->get_future();

        enqueue([task] {
            (*task)();
        });

        //We return the result. We will still have to call .get() on it
        return result;
    }
private:
    std::queue<Task> tasks;
    std::mutex queue_mtx;
    std::condition_variable cv;
    bool stop = false;

    //ORDER MATTERS!! in a class when a destructor runs, declared things are destructed in reverse order from how they are initialized and 
    //in this case workers needs the other variables to not be destructed so that they can finnish their leftover work before being 
    //destructed themselved, thats why we want to destroy workers first aka we need to put them last
    std::vector<std::jthread> workers;
};