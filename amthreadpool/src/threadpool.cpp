#include "amthreadpool/threadpool.hpp"

namespace amthp
{
    threadpool::~threadpool()
    {
        terminate();
    }

    threadpool &threadpool::instance() noexcept
    {
        static threadpool thp;
        return thp;
    }

    void threadpool::init(uint64_t workers_count) noexcept
    {
        std::lock_guard<std::mutex> lock(m_workers_mtx);

        m_running = true;

        m_workers.reserve(workers_count);
        for (uint64_t i = 0; i < workers_count; ++i) {
            m_workers.emplace_back(&threadpool::run_worker, this);
        }
    }

    void threadpool::terminate() noexcept
    {
        std::lock_guard<std::mutex> lock(m_workers_mtx);

        m_running = false;

        m_tasks_cv.notify_all();
        for (std::thread& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        m_workers.resize(0);
    }

    void threadpool::wait(task_id task_id) const noexcept
    {
        std::unique_lock<std::mutex> lock(m_completed_task_ids_mtx);
    
        m_completed_task_ids_cv.wait(lock, [this, task_id]()->bool {
            return m_completed_task_ids.find(task_id) != m_completed_task_ids.end();
        });
    }

    void threadpool::wait_all() const noexcept
    {
        std::unique_lock<std::mutex> lock(m_completed_task_ids_mtx);
    
        m_completed_task_ids_cv.wait(lock, [this]() -> bool {
            std::lock_guard<std::mutex> task_lock(m_tasks_mtx);
            return m_tasks.empty() && m_last_task_id == m_completed_task_ids.size();
        });
    }

    bool threadpool::task_finished(task_id task_id) const noexcept
    {
        std::lock_guard<std::mutex> lock(m_completed_task_ids_mtx);
        return m_completed_task_ids.find(task_id) != m_completed_task_ids.end();
    }

    bool threadpool::is_running() const noexcept
    {
        return m_running;
    }

    void threadpool::run_worker()
    {
        while (true) {
            std::unique_lock<std::mutex> lock(m_tasks_mtx);
            m_tasks_cv.wait(lock, [this]() -> bool { return !m_tasks.empty() || !m_running; });

            if (!m_running) {
                break;
            }

            if (!m_tasks.empty()) {
                auto task = std::move(m_tasks.front());
                m_tasks.pop();
                lock.unlock();
                
                task.first.get();

                std::lock_guard<std::mutex> completed_lock(m_completed_task_ids_mtx);
                m_completed_task_ids.insert(task.second);
                m_completed_task_ids_cv.notify_all();
            }
        }
    }
}