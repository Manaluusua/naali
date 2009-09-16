// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "Foundation.h"
#include "ThreadTaskManager.h"

namespace Foundation
{

    ThreadTaskManager::ThreadTaskManager(Framework* framework) :
        framework_(framework)
    {
    }

    ThreadTaskManager::~ThreadTaskManager()
    {
        std::vector<ThreadTaskPtr>::iterator i = tasks_.begin();
        while (i != tasks_.end())
        {
            (*i)->Stop();
            (*i)->SetThreadTaskManager(0);
            ++i;
        }
    }

    void ThreadTaskManager::AddThreadTask(ThreadTaskPtr task)
    {
        std::vector<ThreadTaskPtr>::iterator i = tasks_.begin();
        while (i != tasks_.end())
        {
            if ((*i) == task)
            {
                RootLogWarning("Thread task " + task->GetTaskDescription() + " already added");
                return; // Already added
            }
            ++i;
        }
        
        task->SetThreadTaskManager(this);
        tasks_.push_back(task);
    }

    void ThreadTaskManager::RemoveThreadTask(ThreadTaskPtr task)
    {
        std::vector<ThreadTaskPtr>::iterator i = tasks_.begin();
        while (i != tasks_.end())
        {
            if ((*i) == task)
            {
                (*i)->Stop();
                (*i)->SetThreadTaskManager(0);
                tasks_.erase(i);
                return;
            }
            ++i;
        }
        
        RootLogWarning("Can not remove thread task " + task->GetTaskDescription() + ", not found");
    }
    
    void ThreadTaskManager::RemoveThreadTask(const std::string& task_description)
    {
        std::vector<ThreadTaskPtr>::iterator i = tasks_.begin();
        while (i != tasks_.end())
        {
            if ((*i)->GetTaskDescription() == task_description)
            {
                (*i)->Stop();
                (*i)->SetThreadTaskManager(0);
                tasks_.erase(i);
                return;
            }
            ++i;
        }
        
        RootLogWarning("Can not remove thread task " + task_description + ", not found");
    }

    Core::request_tag_t ThreadTaskManager::AddRequest(const std::string& task_description, ThreadTaskRequestPtr request)
    {
        if (request)
        {
            std::vector<ThreadTaskPtr>::iterator i = tasks_.begin();
            while (i != tasks_.end())
            {
                if ((*i)->GetTaskDescription() == task_description)
                {
                    Core::request_tag_t tag = framework_->GetEventManager()->GetNextRequestTag();
                    request->tag_ = tag;
                    (*i)->AddRequest(request);
                    return tag;
                }
                ++i;
            }
            
            RootLogError("No thread task matching task description " + task_description + ", could not queue request");
        }
        else
        {
            RootLogError("Null request passed to AddRequest");
        }
        
        return 0;
    }
    
    void ThreadTaskManager::QueueResult(ThreadTaskResultPtr result)
    {
        Core::MutexLock lock(result_mutex_);
        results_.push_back(result);
    }

    void ThreadTaskManager::SendResultEvents()
    {
        std::vector<ThreadTaskResultPtr> results = GetResults();
        EventManagerPtr event_manager = framework_->GetEventManager();
        Core::event_category_id_t threadtask_category = event_manager->QueryEventCategory("Task");
        
        std::vector<ThreadTaskResultPtr>::iterator i = results.begin();
        while (i != results.end())
        {
            event_manager->SendEvent(threadtask_category, Task::Events::REQUEST_COMPLETED, (*i).get());
            ++i;
        }
    }

    std::vector<ThreadTaskResultPtr> ThreadTaskManager::GetResults()
    {
        std::vector<ThreadTaskResultPtr> results;
        
        // Get non-queued results from tasks, delete finished tasks
        std::vector<ThreadTaskPtr>::iterator i = tasks_.begin();
        while (i != tasks_.end())
        {
            if ((*i)->HasFinished())
            {
                ThreadTaskResultPtr result = (*i)->GetResult();
                if (result)
                    results.push_back(result);
                i = tasks_.erase(i);
            }
            else ++i;
        }
        
        // Add queued results
        {
            Core::MutexLock lock(result_mutex_);

            std::list<ThreadTaskResultPtr>::iterator i = results_.begin();
            while (i != results_.end())
            {
                results.push_back(*i);
                ++i;
            }
            
            results_.clear();
        }
        
        return results;
    }

    std::vector<ThreadTaskResultPtr> ThreadTaskManager::GetResults(const std::string& task_description)
    {
        std::vector<ThreadTaskResultPtr> results;
        
        // Get non-queued results from tasks, delete finished tasks
        std::vector<ThreadTaskPtr>::iterator i = tasks_.begin();
        while (i != tasks_.end())
        {
            if (((*i)->GetTaskDescription() == task_description) && ((*i)->HasFinished()))
            {
                ThreadTaskResultPtr result = (*i)->GetResult();
                if (result)
                    results.push_back(result);
                i = tasks_.erase(i);
            }
            else ++i;
        }
        
        // Add queued results
        {
            Core::MutexLock lock(result_mutex_);

            std::list<ThreadTaskResultPtr>::iterator i = results_.begin();
            while (i != results_.end())
            {
                if ((*i)->task_description_ == task_description)
                {
                    results.push_back(*i);
                    i = results_.erase(i);
                }
                else ++i;
            }
        }
        
        return results;
    }

    Core::uint ThreadTaskManager::GetNumResults()
    {
        Core::MutexLock lock(result_mutex_);
        return results_.size();
    }
    
    Core::uint ThreadTaskManager::GetNumResults(const std::string& task_description)
    {
        Core::uint num = 0;
        
        {
            Core::MutexLock lock(result_mutex_);
            std::list<ThreadTaskResultPtr>::iterator i = results_.begin();
            while (i != results_.end())
            {
                if ((*i)->task_description_ == task_description)
                    ++num;
                ++i;
            }
        }
        
        return num;
    }
}