#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

#include <vector>
#include <unordered_set>
#include <queue>

namespace amthp
{
    class threadpool final 
    {
    public:
        using task_id = uint64_t;

    public:
        static threadpool& instance() noexcept;

        void init(uint64_t workers_count) noexcept;
        void terminate() noexcept;

        void wait(task_id task_id) const noexcept;
        void wait_all() const noexcept;

        bool task_finished(task_id task_id) const noexcept;
        bool is_running() const noexcept;

        template <typename Func, typename... Args>
        task_id add_task(const Func& func, Args&&... args) noexcept
        {
            task_id id = m_last_task_id++;
        
            std::lock_guard<std::mutex> tasks_lock(m_tasks_mtx);
            m_tasks.emplace(std::async(std::launch::deferred, func, std::forward<Args>(args)...), id);

            m_tasks_cv.notify_one();
            return id;
        }

        threadpool(threadpool&& thp) noexcept = default;
        threadpool& operator=(threadpool&& thp) noexcept = default;

        threadpool(const threadpool& thp) = delete;
        threadpool& operator=(const threadpool& thp) = delete;

    private:
        threadpool() = default;
        ~threadpool();

        void run_worker();

    private:
        using task = std::pair<std::future<void>, task_id>;

    private:
        std::vector<std::thread> m_workers;
        mutable std::mutex m_workers_mtx;
        
        std::queue<task> m_tasks;
        mutable std::mutex m_tasks_mtx;
        mutable std::condition_variable m_tasks_cv;

        std::unordered_set<task_id> m_completed_task_ids; 
        mutable std::mutex m_completed_task_ids_mtx;
        mutable std::condition_variable m_completed_task_ids_cv;

        std::atomic_bool m_running = false;
        std::atomic<task_id> m_last_task_id = 0;
    };
}