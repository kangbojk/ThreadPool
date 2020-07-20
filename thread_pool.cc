#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <queue>
#include <future>
#include <iostream>

class ThreadPool
{
public:
    ThreadPool(int size = std::thread::hardware_concurrency()) : stop(false), busy(0)
    {
        for (size_t i = 0; i < size; i++)
        {
            workers.push_back(std::thread(&ThreadPool::start_thread, this));
        }
    }

    template <typename Function, typename... Args>
    void push_task(Function &&f, Args &&... args)
    {
        // decltype?
        using task_return_type = typename std::result_of<Function(Args...)>::type;

        auto task = std::packaged_task<task_return_type()>(std::bind(std::forward<Function &&>(f), std::forward<Args &&...>(args)...));
        task_queue.push(std::move(task));
        cv.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lk(m);
        cv_finished.wait(lk, [this]() {
            return task_queue.empty() && (busy == 0);
        });
    }

    void task_queue_test()
    {
        int size = task_queue.size();
        for (int i = 0; i < size; ++i)
        {
            auto t = std::move(task_queue.front());
            task_queue.pop();
            t();
        }
    }

    ~ThreadPool()
    {
        std::unique_lock<std::mutex> lk(m);
        stop = true;
        cv.notify_all();
        lk.unlock();

        for (size_t i = 0; i < workers.size(); i++)
            workers.at(i).join();
    }

private:
    void start_thread()
    {
        while (!stop)
        {
            std::unique_lock<std::mutex> lk(m);
            cv.wait(lk, [this]() {
                return stop || !task_queue.empty();
            });

            if (!task_queue.empty())
            {
                ++busy;

                auto task = std::move(task_queue.front());
                task_queue.pop();

                lk.unlock();
                task();
                lk.lock();
                --busy;

                cv_finished.notify_one();
            }
        }
    }

    std::vector<std::thread> workers;
    std::mutex m;
    std::condition_variable cv, cv_finished;
    std::queue<std::packaged_task<void()>> task_queue;
    bool stop;
    uint32_t busy;
};

int main()
{
    ThreadPool tp;

    auto sayHi = [](int i) {
        std::cout << "hi there " << i << std::endl;
    };

    for (size_t i = 0; i < 3; i++)
    {
        tp.push_task(sayHi, i);
    }

    tp.wait();
}