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
        m_workers.reserve(workers_count);
        for (uint64_t i = 0; i < workers_count; ++i) {
            m_workers.emplace_back(&threadpool::run_worker, this);
        }
    }

    void threadpool::terminate() noexcept
    {
        m_running.store(false);

        for (std::thread& worker : m_workers) {
            m_tasks_cv.notify_all();
            
            if (worker.joinable()) {
                worker.join();
            }
        }
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

    void threadpool::run_worker()
    {
        while (m_running) {
            std::unique_lock<std::mutex> lock(m_tasks_mtx);
            m_tasks_cv.wait(lock, [this]() -> bool { return !m_tasks.empty() || !m_running; });

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